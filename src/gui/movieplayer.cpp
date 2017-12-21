/*
  Neutrino-GUI  -   DBoxII-Project

  Movieplayer (c) 2003, 2004 by gagga
  Based on code by Dirch, obi and the Metzler Bros. Thanks.
  (C) 2010-2014 Stefan Seyfried

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

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stdint.h>
#include <global.h>
#include <neutrino.h>

#include <gui/audiomute.h>
#include <gui/audio_select.h>
#include <gui/epgview.h>
#include <gui/eventlist.h>
#include <gui/movieplayer.h>
#include <gui/osd_helpers.h>
#include <gui/infoviewer.h>
#include <gui/timeosd.h>
#include <gui/widget/helpbox.h>
#include <gui/infoclock.h>
#include <gui/plugins.h>
#include <gui/videosettings.h>
#include <gui/streaminfo2.h>
#include <gui/lua/luainstance.h>
#include <gui/lua/lua_video.h>
#include <gui/screensaver.h>
#include <driver/screenshot.h>
#include <driver/volume.h>
#include <driver/display.h>
#include <driver/abstime.h>
#include <driver/record.h>
#include <driver/fontrenderer.h>
#include <eitd/edvbstring.h>
#include <system/helpers.h>
#include <system/helpers-json.h>

#include <src/mymenu.h>

#include <unistd.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <sys/mount.h>
#include <json/json.h>

#include <video.h>
#include <libtuxtxt/teletext.h>
#include <zapit/zapit.h>
#include <system/set_threadname.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iconv.h>
#include <libdvbsub/dvbsub.h>
#include <audio.h>
#ifdef ENABLE_GRAPHLCD
#include <driver/nglcd.h>
bool glcd_play = false;
#endif
#include <gui/widget/stringinput_ext.h>
#include <gui/screensetup.h>
#include <gui/widget/msgbox.h>
#if HAVE_SPARK_HARDWARE
#include <libavcodec/avcodec.h>
#endif

#include <system/stacktrace.h>

#if 0
#include <gui/infoicons.h>
#endif

extern cVideo * videoDecoder;
extern cAudio * audioDecoder;
extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */

extern CVolume* g_volume;
extern CTimeOSD *FileTimeOSD;

#define TIMESHIFT_SECONDS 3
#define ISO_MOUNT_POINT "/media/iso"
#define MUTE true
#define NO_MUTE false

CMoviePlayerGui* CMoviePlayerGui::instance_mp = NULL;
CMoviePlayerGui* CMoviePlayerGui::instance_bg = NULL;
OpenThreads::Mutex CMoviePlayerGui::mutex;
OpenThreads::Mutex CMoviePlayerGui::bgmutex;
OpenThreads::Condition CMoviePlayerGui::cond;
pthread_t CMoviePlayerGui::bgThread;
cPlayback *CMoviePlayerGui::playback;
bool CMoviePlayerGui::webtv_started;
CMovieBrowser* CMoviePlayerGui::moviebrowser;
CBookmarkManager * CMoviePlayerGui::bookmarkmanager;

CMoviePlayerGui& CMoviePlayerGui::getInstance(bool background)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (!instance_mp )
	{
		instance_mp = new CMoviePlayerGui();
		instance_bg = new CMoviePlayerGui();
		printf("[neutrino CMoviePlayerGui] Instance created...\n");
	}
	return background ? *instance_bg : *instance_mp;
}

CMoviePlayerGui::CMoviePlayerGui()
{
	Init();
}

CMoviePlayerGui::~CMoviePlayerGui()
{
	if (this == instance_mp)
		stopPlayBack();
	if(moviebrowser){
		delete moviebrowser;
		moviebrowser = NULL;
	}
	if(filebrowser){
		delete filebrowser;
		filebrowser = NULL;
	}
	if(bookmarkmanager){
		delete bookmarkmanager;
		bookmarkmanager = NULL;
	}
	if(playback){
		delete playback;
		playback = NULL;
	}
	if (this == instance_mp) {
		delete instance_bg;
		instance_bg = NULL;
	}
	instance_mp = NULL;
	filelist.clear();
}

#if !HAVE_COOL_HARDWARE
// used by libdvbsub/dvbsub.cpp
void getPlayerPts(int64_t *pts)
{
	cPlayback *playback = CMoviePlayerGui::getInstance().getPlayback();
	if (playback)
		playback->GetPts((uint64_t &) *pts);
}
#endif

void CMoviePlayerGui::Init(void)
{
	playing = false;
	stopped = true;
	currentVideoSystem = -1;
	currentOsdResolution = 0;

	frameBuffer = CFrameBuffer::getInstance();

	if (playback == NULL)
		playback = new cPlayback(0);
	if (moviebrowser == NULL)
		moviebrowser = new CMovieBrowser();
	if (bookmarkmanager == NULL)
		bookmarkmanager = new CBookmarkManager();

	// video files
	filefilter_video.addFilter("ts");
#if !HAVE_TRIPLEDRAGON
	filefilter_video.addFilter("asf");
	filefilter_video.addFilter("avi");
	filefilter_video.addFilter("mkv");
#endif
	filefilter_video.addFilter("flv");
	filefilter_video.addFilter("iso");
	filefilter_video.addFilter("m2p");
	filefilter_video.addFilter("m2ts");
	filefilter_video.addFilter("mov");
	filefilter_video.addFilter("mp4");
	filefilter_video.addFilter("mpeg");
	filefilter_video.addFilter("mpg");
	filefilter_video.addFilter("mpv");
	filefilter_video.addFilter("pls");
	filefilter_video.addFilter("trp");
	filefilter_video.addFilter("vdr");
	filefilter_video.addFilter("vob");
	filefilter_video.addFilter("wmv");
	// video playlists
	filefilter_video.addFilter("m3u");
	filefilter_video.addFilter("m3u8");

	// audio files
	filefilter_audio.addFilter("aac");
	filefilter_audio.addFilter("aif");
	filefilter_audio.addFilter("aiff");
	filefilter_audio.addFilter("cdr");
	filefilter_audio.addFilter("dts");
	filefilter_audio.addFilter("flac");
	filefilter_audio.addFilter("flv");
	filefilter_audio.addFilter("m2a");
	filefilter_audio.addFilter("m4a");
	filefilter_audio.addFilter("mp2");
	filefilter_audio.addFilter("mp3");
	filefilter_audio.addFilter("mpa");
	filefilter_audio.addFilter("ogg");
	filefilter_audio.addFilter("wav");
	// audio playlists
	filefilter_audio.addFilter("m3u");
	filefilter_audio.addFilter("m3u8");

	if (g_settings.network_nfs_moviedir.empty())
		Path_local = "/";
	else
		Path_local = g_settings.network_nfs_moviedir;

	if (g_settings.filebrowser_denydirectoryleave)
		filebrowser = new CFileBrowser(Path_local.c_str());
	else
		filebrowser = new CFileBrowser();

	// filebrowser->Filter is set in exec() function
	filebrowser->Hide_records = true;
	filebrowser->Multi_Select = true;
	filebrowser->Dirs_Selectable = true;

	speed = 1;
	timeshift = TSHIFT_MODE_OFF;
	numpida = 0;
	numpids = 0;
	numpidt = 0;
	showStartingHint = false;

	iso_file = false;
#if 0
	lock_subs = false;
#endif
	bgThread = 0;
	info_1 = "";
	info_2 = "";
	filelist_it = filelist.end();
	vzap_it = filelist_it;
	fromInfoviewer = false;
	keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_NORMAL;
	isLuaPlay = false;
	haveLuaInfoFunc = false;
	blockedFromPlugin = false;

	CScreenSaver::getInstance()->resetIdleTime();
}

void CMoviePlayerGui::cutNeutrino()
{
	printf("%s: playing %d isUPNP %d\n", __func__, playing, isUPNP);
	if (playing)
		return;

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	COsdHelpers *coh     = COsdHelpers::getInstance();
	currentVideoSystem   = coh->getVideoSystem();
	currentOsdResolution = coh->getOsdResolution();
#endif

	playing = true;
	/* set g_InfoViewer update timer to 1 sec, should be reset to default from restoreNeutrino->set neutrino mode  */
	if (!isWebChannel)
		g_InfoViewer->setUpdateTimer(1000 * 1000);

	if (isUPNP)
		return;

#if 0
	CZapit::getInstance()->setMoviePlayer(true);// let CCamManager::SetMode know, the call is from MoviePlayer
#endif
	g_Zapit->lockPlayBack();

#ifdef HAVE_AZBOX_HARDWARE
	/* we need sectionsd to get idle and zapit to release the demuxes
	 * and decoders so that the external player can do its work
	 * TODO: what about timeshift? */
	g_Sectionsd->setServiceChanged(0, false);
	g_Zapit->setStandby(true);
#endif

	int new_mode = NeutrinoModes::mode_unknown;
	m_LastMode = CNeutrinoApp::getInstance()->getMode();
	printf("%s: old mode %d\n", __func__, m_LastMode);fflush(stdout);
	if (isWebChannel)
	{
		bool isRadioMode = (m_LastMode == NeutrinoModes::mode_radio || m_LastMode == NeutrinoModes::mode_webradio);
		new_mode = (isRadioMode) ? NeutrinoModes::mode_webradio : NeutrinoModes::mode_webtv;
		m_LastMode |= NeutrinoModes::norezap;
	}
	else
	{
		new_mode = NeutrinoModes::mode_ts;
	}
	printf("%s: new mode %d\n", __func__, new_mode);fflush(stdout);
	printf("%s: save mode %x\n", __func__, m_LastMode);fflush(stdout);
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoModes::norezap | new_mode);
}

void CMoviePlayerGui::restoreNeutrino()
{
	printf("%s: playing %d isUPNP %d\n", __func__, playing, isUPNP);
	if (!playing)
		return;

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	if ((currentVideoSystem > -1) &&
	    (g_settings.video_Mode == VIDEO_STD_AUTO) &&
	    (g_settings.enabled_auto_modes[currentVideoSystem] == 1)) {
		COsdHelpers *coh = COsdHelpers::getInstance();
		if (currentVideoSystem != coh->getVideoSystem()) {
			coh->setVideoSystem(currentVideoSystem, false);
			coh->changeOsdResolution(currentOsdResolution, false, true);
		}
		currentVideoSystem = -1;
	}
#endif

	playing = false;
#ifdef HAVE_AZBOX_HARDWARE
	g_Zapit->setStandby(false);
	CZapit::getInstance()->SetVolume(CZapit::getInstance()->GetVolume());
#endif

	if (isUPNP)
		return;
#if ! HAVE_COOL_HARDWARE
	g_Zapit->unlockPlayBack();
#else
	CZapit::getInstance()->EnablePlayback(true);
#endif
	printf("%s: restore mode %x\n", __func__, m_LastMode);fflush(stdout);
#if 0
	if (m_LastMode == NeutrinoModes::mode_tv)
		g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x200, false);
#endif
	if (m_LastMode != NeutrinoModes::mode_unknown)
		CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, m_LastMode);

#if 0
	if (m_LastMode == NeutrinoModes::mode_tv) {
		CZapitChannel *channel = CZapit::getInstance()->GetCurrentChannel();
		if (channel && channel->scrambled)
			CZapit::getInstance()->Rezap();
	}
#endif
	printf("%s: restoring done.\n", __func__);fflush(stdout);
}

static bool running = false;
int CMoviePlayerGui::exec(CMenuTarget * parent, const std::string & actionKey)
{
	printf("[movieplayer] actionKey=%s\n", actionKey.c_str());
	if (running)
		return menu_return::RETURN_EXIT_ALL;
	running = true;

	if (parent)
		parent->hide();


	if (actionKey == "fileplayback_video" || actionKey == "fileplayback_audio" || actionKey == "tsmoviebrowser")
	{
		if (actionKey == "fileplayback_video") {
			printf("[movieplayer] wakeup_hdd(%s) for %s\n", g_settings.network_nfs_moviedir.c_str(), actionKey.c_str());
			wakeup_hdd(g_settings.network_nfs_moviedir.c_str());
		}
		else if (actionKey == "fileplayback_audio") {
			printf("[movieplayer] wakeup_hdd(%s) for %s\n", g_settings.network_nfs_audioplayerdir.c_str(), actionKey.c_str());
			wakeup_hdd(g_settings.network_nfs_audioplayerdir.c_str());
		}
		else {
			printf("[movieplayer] wakeup_hdd(%s) for %s\n", g_settings.network_nfs_recordingdir.c_str(), actionKey.c_str());
			wakeup_hdd(g_settings.network_nfs_recordingdir.c_str());
		}
	}

	if (!access(MOVIEPLAYER_START_SCRIPT, X_OK)) {
		puts("[movieplayer.cpp] executing " MOVIEPLAYER_START_SCRIPT ".");
		if (my_system(MOVIEPLAYER_START_SCRIPT) != 0)
			perror(MOVIEPLAYER_START_SCRIPT " failed");
	}

	Cleanup();
	ClearFlags();
	ClearQueue();

	FileTimeOSD->kill();
	FileTimeOSD->setMode(CTimeOSD::MODE_HIDE);
	FileTimeOSD->setMpTimeForced(false);
	time_forced = false;

	if (actionKey == "tsmoviebrowser") {
		isMovieBrowser = true;
		moviebrowser->setMode(MB_SHOW_RECORDS);
		//wakeup_hdd(g_settings.network_nfs_recordingdir.c_str());
	}
	else if (actionKey == "ytplayback") {
		isMovieBrowser = true;
		moviebrowser->setMode(MB_SHOW_YT);
		isYT = true;
	}
	else if (actionKey == "fileplayback_video") {
		is_audio_player = false;
		if (filebrowser)
			filebrowser->Filter = &filefilter_video;
		//wakeup_hdd(g_settings.network_nfs_moviedir.c_str());
	}
	else if (actionKey == "fileplayback_audio") {
		is_audio_player = true;
		if (filebrowser)
			filebrowser->Filter = &filefilter_audio;
		//wakeup_hdd(g_settings.network_nfs_audioplayerdir.c_str());
	}
	else if (actionKey == "timeshift") {
		timeshift = TSHIFT_MODE_ON;
	}
	else if (actionKey == "ptimeshift") {
		timeshift = TSHIFT_MODE_PAUSE;
	}
	else if (actionKey == "rtimeshift") {
		timeshift = TSHIFT_MODE_REWIND;
	}
#if 0 // TODO ?
	else if (actionKey == "bookmarkplayback") {
		isBookmark = true;
	}
#endif
	else if (actionKey == "netstream") {
		isHTTP = true;
		p_movie_info = NULL;
		is_file_player = true;
		PlayFile();
	}
	else if (actionKey == "upnp") {
		isUPNP = true;
		is_file_player = true;
		PlayFile();
	}
	else if (actionKey == "http") {
		isHTTP = true;
		is_file_player = true;
		PlayFile();
	}
	else if (actionKey == "http_lua") {
		isHTTP = true;
		isLuaPlay = true;
		is_file_player = true;
		PlayFile();
		haveLuaInfoFunc = false;
	}
	else {
		running = false;
		return menu_return::RETURN_REPAINT;
	}

	while(!isHTTP && !isUPNP && SelectFile()) {
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		CVFD::getInstance()->showServicename(file_name.c_str());
		if (timeshift != TSHIFT_MODE_OFF) {
			CVFD::getInstance()->ShowIcon(FP_ICON_TIMESHIFT, true);
			PlayFile();
			CVFD::getInstance()->ShowIcon(FP_ICON_TIMESHIFT, false);
			break;
		}
		do {
#if ! HAVE_COOL_HARDWARE
			is_file_player = true;
#endif
			PlayFile();
		}
		while (repeat_mode || filelist_it != filelist.end());
	}
	CVFD::getInstance()->showServicename(CVFD::getInstance()->getServicename());

	bookmarkmanager->flush();

	if (!access(MOVIEPLAYER_END_SCRIPT, X_OK)) {
		puts("[movieplayer.cpp] executing " MOVIEPLAYER_END_SCRIPT ".");
		if (my_system(MOVIEPLAYER_END_SCRIPT) != 0)
			perror(MOVIEPLAYER_END_SCRIPT " failed");
	}

	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	running = false;

	if (timeshift != TSHIFT_MODE_OFF){
		timeshift = TSHIFT_MODE_OFF;
		return menu_return::RETURN_EXIT_ALL;
	}
	return menu_ret;
}

void CMoviePlayerGui::updateLcd()
{
#if !HAVE_SPARK_HARDWARE
	char tmp[20];
	std::string lcd;
	std::string name;

	if (isMovieBrowser && p_movie_info && !p_movie_info->epgTitle.empty() && p_movie_info->epgTitle.size() && strncmp(p_movie_info->epgTitle.c_str(), "not", 3))
		name = p_movie_info->epgTitle;
	else
		name = pretty_name;

	switch (playstate) {
		case CMoviePlayerGui::PAUSE:
			if (speed < 0) {
				sprintf(tmp, "%dx<| ", abs(speed));
				lcd = tmp;
			} else if (speed > 0) {
				sprintf(tmp, "%dx|> ", abs(speed));
				lcd = tmp;
			} else
				lcd = "|| ";
			break;
		case CMoviePlayerGui::REW:
			sprintf(tmp, "%dx<< ", abs(speed));
			lcd = tmp;
			break;
		case CMoviePlayerGui::FF:
			sprintf(tmp, "%dx>> ", abs(speed));
			lcd = tmp;
			break;
		case CMoviePlayerGui::PLAY:
			lcd = "> ";
			break;
		default:
			break;
	}
	lcd += name;
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
	CVFD::getInstance()->showMenuText(0, lcd.c_str(), -1, true);
#endif
}

void CMoviePlayerGui::fillPids()
{
	if (p_movie_info == NULL)
		return;

	vpid = p_movie_info->VideoPid;
	vtype = p_movie_info->VideoType;
	numpida = 0; currentapid = 0;
	numpids = 0;
	numpidt = 0;
	currentttxsub = "";
	/* FIXME: better way to detect TS recording */
	if (!p_movie_info->audioPids.empty()) {
		currentapid = p_movie_info->audioPids[0].AudioPid;
		currentac3 = p_movie_info->audioPids[0].atype;
	} else if (!vpid) {
		is_file_player = true;
		return;
	}
	for (unsigned int i = 0; i < p_movie_info->audioPids.size(); i++) {
		unsigned int j;
		for (j = 0; j < numpida && p_movie_info->audioPids[i].AudioPid != apids[j]; j++);
		if (j == numpida) {
			apids[i] = p_movie_info->audioPids[i].AudioPid;
			ac3flags[i] = p_movie_info->audioPids[i].atype;
			numpida++;
			if (p_movie_info->audioPids[i].selected) {
				currentapid = p_movie_info->audioPids[i].AudioPid;
				currentac3 = p_movie_info->audioPids[i].atype;
			}
			if (numpida == REC_MAX_APIDS)
				break;
		}
	}
}

void CMoviePlayerGui::Cleanup()
{
	/*clear audiopids */
	for (unsigned int i = 0; i < REC_MAX_APIDS; i++) {
		apids[i] = 0;
		ac3flags[i] = 0;
		language[i].clear();
	}
	numpida = 0; currentapid = 0;
	// clear subtitlepids
	for (unsigned int i = 0; i < REC_MAX_SPIDS; i++) {
		spids[i] = 0;
		slanguage[i].clear();
	}
	numpids = 0;
	// clear teletextpids
	for (unsigned int i = 0; i < REC_MAX_TPIDS; i++) {
		tpids[i] = 0;
		tlanguage[i].clear();
	}
	numpidt = 0; currentttxsub = "";
	vpid = 0;
	vtype = 0;

	startposition = -1;
	is_file_player = false;
	p_movie_info = NULL;
	autoshot_done = false;
	currentaudioname = "Unk";
}

void CMoviePlayerGui::ClearFlags()
{
	isMovieBrowser = false;
	isBookmark = false;
	isHTTP = false;
	isLuaPlay = false;
	isUPNP = false;
	isWebChannel = false;
	isYT = false;
	is_file_player = false;
	is_audio_player = false;
	timeshift = TSHIFT_MODE_OFF;
}

void CMoviePlayerGui::ClearQueue()
{
	repeat_mode = REPEAT_OFF;
	filelist.clear();
	filelist_it = filelist.end();
	milist.clear();
}


void CMoviePlayerGui::enableOsdElements(bool mute)
{
	if (mute)
		CAudioMute::getInstance()->enableMuteIcon(true);

	CInfoClock::getInstance()->enableInfoClock(true);
}

void CMoviePlayerGui::disableOsdElements(bool mute)
{
	if (mute)
		CAudioMute::getInstance()->enableMuteIcon(false);

	CInfoClock::getInstance()->enableInfoClock(false);
}

void CMoviePlayerGui::makeFilename()
{
	if (pretty_name.empty()) {
		std::string::size_type pos = file_name.find_last_of('/');
		if (pos != std::string::npos) {
			pretty_name = file_name.substr(pos+1);
			std::replace(pretty_name.begin(), pretty_name.end(), '_', ' ');
		} else
			pretty_name = file_name;

		if (pretty_name.substr(0,14)=="videoplayback?") {//youtube name
			if (!p_movie_info->epgTitle.empty())
				pretty_name = p_movie_info->epgTitle;
			else
				pretty_name = "";
		}
		printf("CMoviePlayerGui::makeFilename: file_name [%s] pretty_name [%s]\n", file_name.c_str(), pretty_name.c_str());
	}
}

bool CMoviePlayerGui::prepareFile(CFile *file)
{
	bool ret = true;

	numpida = 0; currentapid = 0;
//	currentspid = -1;
//	numsubs = 0;
	autoshot_done = 0;
	if (file->Url.empty())
		file_name = file->Name;
	else {
		file_name = file->Url;
		pretty_name = file->Name;
	}
	if (isMovieBrowser) {
		if (filelist_it != filelist.end()) {
			unsigned idx = filelist_it - filelist.begin();
			if(milist.size() > idx){
				p_movie_info = milist[idx];
				startposition = p_movie_info->bookmarks.start > 0 ? p_movie_info->bookmarks.start*1000 : -1;
				printf("CMoviePlayerGui::prepareFile: file %s start %d\n", file_name.c_str(), startposition);
			}
		}
#if HAVE_COOL_HARDWARE
		if (isYT) {
			file_name = file->Url;
			is_file_player = true;
		}
#endif
		fillPids();
	}
	if (file->getType() == CFile::FILE_ISO)
		ret = mountIso(file);
	//else if (file->getType() == CFile::FILE_PLAYLIST)
		//parsePlaylist(file);

	if (ret)
		makeFilename();
	return ret;
}

bool CMoviePlayerGui::SelectFile()
{
	bool ret = false;
	menu_ret = menu_return::RETURN_REPAINT;

	Cleanup();
	info_1 = "";
	info_2 = "";
	pretty_name.clear();
	file_name.clear();
	cookie_header.clear();
	//reinit Path_local for webif reloadsetup
	Path_local = "/";
	if (is_audio_player)
	{
		if (!g_settings.network_nfs_audioplayerdir.empty())
			Path_local = g_settings.network_nfs_audioplayerdir;
	}
	else
	{
		if (!g_settings.network_nfs_moviedir.empty())
			Path_local = g_settings.network_nfs_moviedir;
	}

	printf("CMoviePlayerGui::SelectFile: isBookmark %d timeshift %d isMovieBrowser %d is_audio_player %d\n", isBookmark, timeshift, isMovieBrowser, is_audio_player);
#if 0
	wakeup_hdd(g_settings.network_nfs_recordingdir.c_str());
#endif
	if (timeshift != TSHIFT_MODE_OFF) {
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		p_movie_info = CRecordManager::getInstance()->GetMovieInfo(live_channel_id);
		file_name = CRecordManager::getInstance()->GetFileName(live_channel_id) + ".ts";
		fillPids();
		makeFilename();
		ret = true;
	}
#if 0 // TODO
	else if (isBookmark) {
		const CBookmark * theBookmark = bookmarkmanager->getBookmark(NULL);
		if (theBookmark == NULL) {
			bookmarkmanager->flush();
			return false;
		}
		file_name = theBookmark->getUrl();
		sscanf(theBookmark->getTime(), "%lld", &startposition);
		startposition *= 1000;
		ret = true;
	}
#endif
	else if (isMovieBrowser) {
		disableOsdElements(MUTE);
		if (moviebrowser->exec(Path_local.c_str())) {
			Path_local = moviebrowser->getCurrentDir();
			CFile *file =  NULL;
			filelist_it = filelist.end();
			if (moviebrowser->getSelectedFiles(filelist, milist)) {
				filelist_it = filelist.begin();
				p_movie_info = *(milist.begin());
// 				file = &(*filelist_it);
			}
			else if ((file = moviebrowser->getSelectedFile()) != NULL) {
				p_movie_info = moviebrowser->getCurrentMovieInfo();
				startposition = 1000 * moviebrowser->getCurrentStartPos();
				printf("CMoviePlayerGui::SelectFile: file %s start %d apid %X atype %d vpid %x vtype %d\n", file_name.c_str(), startposition, currentapid, currentac3, vpid, vtype);

			}
			if (p_movie_info)
				ret = prepareFile(&p_movie_info->file);
		} else
			menu_ret = moviebrowser->getMenuRet();
		enableOsdElements(MUTE);
	} else { // filebrowser
		disableOsdElements(MUTE);
		while (ret == false && filebrowser->exec(Path_local.c_str()) == true) {
			Path_local = filebrowser->getCurrentDir();
			CFile *file = NULL;
			filelist = filebrowser->getSelectedFiles();
			filelist_it = filelist.end();
			if (!filelist.empty()) {
				filelist_it = filelist.begin();
				file = &(*filelist_it);
			}
			if (file) {
				is_file_player = true;
				if (file->getType() == CFile::FILE_PLAYLIST)
					parsePlaylist(file);
				if (!filelist.empty()) {
					filelist_it = filelist.begin();
					file = &(*filelist_it);
				}
				ret = prepareFile(file);
			}
		}
		menu_ret = filebrowser->getMenuRet();
		enableOsdElements(MUTE);
	}
	if (!is_audio_player)
		g_settings.network_nfs_moviedir = Path_local;

	return ret;
}

void *CMoviePlayerGui::ShowStartHint(void *arg)
{
	set_threadname(__func__);
	CMoviePlayerGui *caller = (CMoviePlayerGui *)arg;
	CHintBox *hintbox = NULL;
	if (!caller->pretty_name.empty()) {
		hintbox = new CHintBox(LOCALE_MOVIEPLAYER_STARTING, caller->pretty_name.c_str(), 450, NEUTRINO_ICON_MOVIEPLAYER);
		hintbox->paint();
	}
	while (caller->showStartingHint) {
		neutrino_msg_t msg;
		neutrino_msg_data_t data;
		g_RCInput->getMsg(&msg, &data, 1);
		if (msg == CRCInput::RC_home || msg == CRCInput::RC_stop) {
			caller->playback->RequestAbort();
		}
#if 0
		else if (caller->isWebChannel) {
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
#endif
		else if (caller->isWebChannel && ((msg == (neutrino_msg_t) g_settings.key_quickzap_up ) || (msg == (neutrino_msg_t) g_settings.key_quickzap_down))) {
			caller->playback->RequestAbort();
			g_RCInput->postMsg(msg, data);
		}
		else if (msg != NeutrinoMessages::EVT_WEBTV_ZAP_COMPLETE && msg != CRCInput::RC_timeout && msg > CRCInput::RC_MaxRC) {
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
		else if ((msg>= CRCInput::RC_WithData) && (msg< CRCInput::RC_WithData+ 0x10000000))
                        delete[] (unsigned char*) data;

	}
	if (hintbox != NULL) {
		hintbox->hide();
		delete hintbox;
	}
	return NULL;
}

bool CMoviePlayerGui::StartWebtv(void)
{
	printf("%s: starting...\n", __func__);fflush(stdout);
	last_read = position = duration = 0;

	cutNeutrino();
#if 0
	clearSubtitle();
#endif

	playback->Open(is_file_player ? PLAYMODE_FILE : PLAYMODE_TS);

	bool res = playback->Start((char *) file_name.c_str(), cookie_header);//url with cookies

	playback->SetSpeed(1);
	if (!res) {
		playback->Close();
	} else {
		getCurrentAudioName(is_file_player, currentaudioname);
		if (is_file_player)
			selectAutoLang();
	}

	return res;
}

void* CMoviePlayerGui::bgPlayThread(void *arg)
{
	set_threadname(__func__);
	CMoviePlayerGui *mp = (CMoviePlayerGui *) arg;
	printf("%s: starting... instance %p\n", __func__, mp);fflush(stdout);

	int eof = 0, pos = 0;
	unsigned char *chid = new unsigned char[sizeof(t_channel_id)];
	*(t_channel_id*)chid = mp->movie_info.channelId;

	bool started = mp->StartWebtv();
	printf("%s: started: %d\n", __func__, started);fflush(stdout);
	bool chidused = false;

	mutex.lock();
	if (!webtv_started)
		started = false;
	else if (!started){
		g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_FAILED, (neutrino_msg_data_t) chid);
		chidused = true;
	}
	webtv_started = started;
	mutex.unlock();

	while(webtv_started) {
		if (mp->playback->GetPosition(mp->position, mp->duration)) {
#if 0
			printf("CMoviePlayerGui::bgPlayThread: position %d duration %d (%d)\n", mp->position, mp->duration, mp->duration-mp->position);
#endif
			if (pos == mp->position)
				eof++;
			else
				eof = 0;
			if (eof > 5) {
				printf("CMoviePlayerGui::bgPlayThread: playback stopped, try to rezap...\n");
				g_RCInput->postMsg(NeutrinoMessages::EVT_WEBTV_ZAP_COMPLETE, (neutrino_msg_data_t) chid);
				chidused = true;
				break;
			}
			pos = mp->position;
		}
		bgmutex.lock();
		int res = cond.wait(&bgmutex, 1000);
		bgmutex.unlock();
		if (res == 0)
			break;
#if 0
		mp->showSubtitle(0);
#endif
	}
	printf("%s: play end...\n", __func__);fflush(stdout);
	mp->PlayFileEnd();
	if(!chidused)
		delete [] chid;

	pthread_exit(NULL);
}

bool CMoviePlayerGui::sortStreamList(livestream_info_t info1, livestream_info_t info2)
{
	return (info1.res1 < info2.res1);
}

bool CMoviePlayerGui::luaGetUrl(const std::string &script, const std::string &file, std::vector<livestream_info_t> &streamList)
{
	CHintBox* box = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_LIVESTREAM_READ_DATA));
	box->paint();

	std::string result_code = "";
	std::string result_string = "";

	std::vector<std::string> args;
	args.push_back(file);
#ifdef ENABLE_LUA
	CLuaInstance *lua = new CLuaInstance();
	lua->runScript(script.c_str(), &args, &result_code, &result_string);
	delete lua;
#endif
	if ((result_code != "0") || result_string.empty()) {
		if (box != NULL) {
			box->hide();
			delete box;
		}
		return false;
	}

	std::string errMsg = "";
	Json::Value root;
	bool ok = parseJsonFromString(result_string, &root, &errMsg);
	if (!ok) {
		printf("Failed to parse JSON\n");
		printf("%s\n", errMsg.c_str());
		if (box != NULL) {
			box->hide();
			delete box;
		}
		return false;
	}

	livestream_info_t info;
	std::string tmp;
	bool haveurl = false;
	if ( !root.isObject() ) {
		for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
			info.url=""; info.name=""; info.header=""; info.bandwidth = 1; info.resolution=""; info.res1 = 1;
			tmp = "0";
			Json::Value object_it = *it;
			for (Json::Value::iterator iti = object_it.begin(); iti != object_it.end(); iti++) {
				std::string name = iti.name();
				if (name=="url") {
					info.url = (*iti).asString();
					haveurl = true;
				} else if (name=="name") {
					info.name = (*iti).asString();
				}
				else if (name=="header") {
					info.header = (*iti).asString();
				}
				else if (name=="band") {
					info.bandwidth  = atoi((*iti).asString().c_str());
				} else if (name=="res1") {
					tmp = (*iti).asString();
					info.res1 = atoi(tmp.c_str());
				} else if (name=="res2") {
					info.resolution = tmp + "x" + (*iti).asString();
				}
			}
			if (haveurl) {
				streamList.push_back(info);
			}
			haveurl = false;
		}
	}
	if (root.isObject()) {
		for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
			info.url=""; info.name=""; info.header=""; info.bandwidth = 1; info.resolution=""; info.res1 = 1;
			tmp = "0";
			std::string name = it.name();
			if (name=="url") {
				info.url = (*it).asString();
				haveurl = true;
			} else if (name=="name") {
				info.name = (*it).asString();
			} else if (name=="header") {
					info.header = (*it).asString();
			} else if (name=="band") {
				info.bandwidth  = atoi((*it).asString().c_str());
			} else if (name=="res1") {
				tmp = (*it).asString();
				info.res1 = atoi(tmp.c_str());
			} else if (name=="res2") {
				info.resolution = tmp + "x" + (*it).asString();
			}
		}
		if (haveurl) {
			streamList.push_back(info);
		}
	}
	/* sort streamlist */
	std::sort(streamList.begin(), streamList.end(), sortStreamList);

	/* remove duplicate resolutions */
	livestream_info_t *_info;
	int res_old = 0;
	for (size_t i = 0; i < streamList.size(); ++i) {
		_info = &(streamList[i]);
		if (res_old == _info->res1)
			streamList.erase(streamList.begin()+i);
		res_old = _info->res1;
	}

	if (box != NULL) {
		box->hide();
		delete box;
	}

	return true;
}

bool CMoviePlayerGui::selectLivestream(std::vector<livestream_info_t> &streamList, int res, livestream_info_t* info)
{
	livestream_info_t* _info;
	int _res = res;

#if 0
	printf("\n");
	for (size_t i = 0; i < streamList.size(); ++i) {
		_info = &(streamList[i]);
		printf("%d - _info->res1: %4d, _info->res: %9s, _info->bandwidth: %d\n", i, _info->res1, (_info->resolution).c_str(), _info->bandwidth);
	}
	printf("\n");
#endif

	bool resIO = false;
	while (!streamList.empty()) {
		size_t i;
		for (i = 0; i < streamList.size(); ++i) {
			_info = &(streamList[i]);
			if (_info->res1 == _res) {
				info->url        = _info->url;
				info->name       = _info->name;
				info->header     = _info->header;
				info->resolution = _info->resolution;
				info->res1       = _info->res1;
				info->bandwidth  = _info->bandwidth;
				return true;
			}
		}
		/* Required resolution not found, decreasing resolution */
		for (i = streamList.size(); i > 0; --i) {
			_info = &(streamList[i-1]);
			if (_info->res1 < _res) {
				_res = _info->res1;
				resIO = true;
				break;
			}
		}
		/* Required resolution not found, increasing resolution */
		if (resIO == false) {
			for (i = 0; i < streamList.size(); ++i) {
				_info = &(streamList[i]);
				if (_info->res1 > _res) {
					_res = _info->res1;
					break;
				}
			}
		}
	}
	return false;
}

bool CMoviePlayerGui::getLiveUrl(const std::string &url, const std::string &script, std::string &realUrl, std::string &_pretty_name, std::string &info1, std::string &info2, std::string &header)
{
	std::vector<livestream_info_t> liveStreamList;
	livestream_info_t info;

	if (script.empty()) {
		realUrl = url;
		return true;
	}
	std::string _script = script;

	if (_script.find("/") == std::string::npos)
	{
		std::string _s = g_settings.livestreamScriptPath + "/" + _script;
		printf("[%s:%s:%d] script: %s\n", __file__, __func__, __LINE__, _s.c_str());
		if (!file_exists(_s.c_str()))
		{
			_s = std::string(WEBTVDIR_VAR) + "/" + _script;
			printf("[%s:%s:%d] script: %s\n", __file__, __func__, __LINE__, _s.c_str());
		}
		if (!file_exists(_s.c_str()))
		{
			_s = std::string(WEBTVDIR) + "/" + _script;
			printf("[%s:%s:%d] script: %s\n", __file__, __func__, __LINE__, _s.c_str());
		}
		_script = _s;
	}
	size_t pos = _script.find(".lua");
	if (!file_exists(_script.c_str()) || (pos == std::string::npos) || (_script.length()-pos != 4)) {
		printf(">>>>> [%s:%s:%d] script error\n", __file__, __func__, __LINE__);
		return false;
	}
	if (!luaGetUrl(_script, url, liveStreamList)) {
		printf(">>>>> [%s:%s:%d] lua script error\n", __file__, __func__, __LINE__);
		return false;
	}

	if (!selectLivestream(liveStreamList, g_settings.livestreamResolution, &info)) {
		printf(">>>>> [%s:%s:%d] error selectLivestream\n", __file__, __func__, __LINE__);
		return false;
	}

	realUrl = info.url;
	if (!info.name.empty()) {
		info1 = info.name;
		_pretty_name = info.name;
	}
	if (!info.header.empty()) {
		header = info.header;
	}

#if 0
	if (!info.resolution.empty())
		info2 = info.resolution;
	if (info.bandwidth > 0) {
		char buf[32];
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf), "%.02f kbps", (float)((float)info.bandwidth/(float)1000));
		info2 += (std::string)", " + (std::string)buf;
	}
#else
	if (info.bandwidth > 0) {
		char buf[32];
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf), "%.02f kbps", (float)((float)info.bandwidth/(float)1000));
		info2 = (std::string)buf;
	}
#endif
	return true;
}

bool CMoviePlayerGui::PlayBackgroundStart(const std::string &file, const std::string &name, t_channel_id chan, const std::string &script)
{
	printf("%s: starting...\n", __func__);fflush(stdout);
	static CZapProtection *zp = NULL;
	if (zp)
		return true;

	if (g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER) {
		int age = -1;
		const char *ages[] = { "18+", "16+", "12+", "6+", "0+", NULL };
		int agen[] = { 18, 16, 12, 6, 0 };
		for (int i = 0; ages[i] && age < 0; i++) {
			const char *n = name.c_str();
			const char *h = n;
			while ((age < 0) && (h = strstr(h, ages[i])))
				if ((h == n) || !isdigit(*(h - 1)))
					age = agen[i];
		}
		if (age > -1 && age >= g_settings.parentallock_lockage && g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER) {
			zp = new CZapProtection(g_settings.parentallock_pincode, age);
			bool unlocked = zp->check();
			delete zp;
			zp = NULL;
			if (!unlocked)
				return false;
		} else {
			CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(chan);
			if (channel && channel->Locked() != g_settings.parentallock_defaultlocked && !CNeutrinoApp::getInstance()->channelList->checkLockStatus(0x100))
				return false;
		}
	}

	std::string realUrl;
	std::string _pretty_name = name;
	cookie_header.clear();
	if (!getLiveUrl(file, script, realUrl, _pretty_name, livestreamInfo1, livestreamInfo2, cookie_header)) {
		return false;
	}

	instance_bg->Cleanup();
	instance_bg->ClearFlags();
	instance_bg->ClearQueue();

	instance_bg->isWebChannel = true;
	instance_bg->is_file_player = true;
	instance_bg->isHTTP = true;
	instance_bg->file_name = realUrl;
	instance_bg->pretty_name = _pretty_name;
	instance_bg->cookie_header = cookie_header;

	instance_bg->movie_info.epgTitle = name;
	instance_bg->movie_info.channelName = realUrl;
	instance_bg->movie_info.channelId = chan;
	instance_bg->p_movie_info = &movie_info;

	numpida = 0; currentapid = 0;
	numpids = 0;
	numpidt = 0; currentttxsub = "";

	stopPlayBack();
	webtv_started = true;
	if (pthread_create (&bgThread, 0, CMoviePlayerGui::bgPlayThread, instance_bg)) {
		printf("ERROR: pthread_create(%s)\n", __func__);
		webtv_started = false;
		return false;
	}

	printf("%s: this %p started, thread %lx\n", __func__, this, bgThread);fflush(stdout);
	return true;
}

bool MoviePlayerZapto(const std::string &file, const std::string &name, t_channel_id chan)
{
	return CMoviePlayerGui::getInstance().PlayBackgroundStart(file, name, chan);
}

extern void MoviePlayerStop(void)
{
	CMoviePlayerGui::getInstance().stopPlayBack();
}

void CMoviePlayerGui::stopPlayBack(void)
{
	printf("%s: stopping...\n", __func__);
	playback->RequestAbort();

	repeat_mode = REPEAT_OFF;
	if (bgThread) {
		printf("%s: this %p join background thread %lx\n", __func__, this, bgThread);fflush(stdout);
		mutex.lock();
		webtv_started = false;
		if(playback)
			playback->RequestAbort();
		mutex.unlock();
		cond.broadcast();
		pthread_join(bgThread, NULL);
		bgThread = 0;
		livestreamInfo1.clear();
		livestreamInfo2.clear();
	}
	printf("%s: stopped\n", __func__);
}

void CMoviePlayerGui::Pause(bool b)
{
	if (b && (playstate == CMoviePlayerGui::PAUSE))
		b = !b;
	if (b) {
		playback->SetSpeed(0);
		playstate = CMoviePlayerGui::PAUSE;
	} else {
		playback->SetSpeed(1);
		playstate = CMoviePlayerGui::PLAY;
	}
}

void CMoviePlayerGui::PlayFile(void)
{
	PlayFileStart();
	PlayFileLoop();
	bool repeat = (repeat_mode == REPEAT_OFF);
	if (isLuaPlay)
		repeat = (!blockedFromPlugin);
	PlayFileEnd(repeat);
}

bool CMoviePlayerGui::PlayFileStart(void)
{
	menu_ret = menu_return::RETURN_REPAINT;

	FileTimeOSD->setMpTimeForced(false);
	time_forced = false;

	position = 0, duration = 0;
	speed = 1;
	last_read = 0;

	printf("%s: starting...\n", __func__);
	stopPlayBack();

	playstate = CMoviePlayerGui::STOPPED;
	printf("Startplay at %d seconds\n", startposition/1000);
	handleMovieBrowser(CRCInput::RC_nokey, position);

	cutNeutrino();
	if (isWebChannel)
		videoDecoder->setBlank(true);

#if 0
	clearSubtitle();
#endif
	playback->SetTeletextPid(-1);

	printf("IS FILE PLAYER: %s\n", is_file_player ?  "true": "false" );
	playback->Open(is_file_player ? PLAYMODE_FILE : PLAYMODE_TS);

	if (p_movie_info) {
		if (timeshift != TSHIFT_MODE_OFF) {
		// p_movie_info may be invalidated by CRecordManager while we're still using it. Create and use a copy.
			movie_info = *p_movie_info;
			p_movie_info = &movie_info;
		}

		duration = p_movie_info->length * 60 * 1000;
		int percent = CZapit::getInstance()->GetPidVolume(p_movie_info->channelId, currentapid, currentac3 == 1);
		CZapit::getInstance()->SetVolumePercent(percent);
#if HAVE_SPARK_HARDWARE
		CScreenSetup cSS;
		cSS.showBorder(p_movie_info->epgId);
#endif
	} else {
#if HAVE_SPARK_HARDWARE
		CScreenSetup cSS;
		cSS.showBorder(0);
#endif
	}

	file_prozent = 0;
#if HAVE_SPARK_HARDWARE
	old3dmode = frameBuffer->get3DMode();
#endif
#ifdef ENABLE_GRAPHLCD
	nGLCD::MirrorOSD(false);
	if (p_movie_info)
		nGLCD::lockChannel(p_movie_info->channelName, p_movie_info->epgTitle);
	else {
		glcd_play = true;
		nGLCD::lockChannel(g_Locale->getText(LOCALE_MOVIEPLAYER_HEAD), file_name.c_str(), file_prozent);
	}
#endif
	pthread_t thrStartHint = 0;
	if (is_file_player) {
		showStartingHint = true;
		pthread_create(&thrStartHint, NULL, CMoviePlayerGui::ShowStartHint, this);
	}
	bool res = playback->Start((char *) file_name.c_str(), vpid, vtype, currentapid, currentac3, duration);

	if (thrStartHint) {
		showStartingHint = false;
		pthread_join(thrStartHint, NULL);
	}

	if (!res) {
		playback->Close();
		repeat_mode = REPEAT_OFF;
		return false;
	} else {
		numpida = 0;
		playback->FindAllPids(apids, ac3flags, &numpida, language);
		if (p_movie_info){
			if(!numpida && !p_movie_info->audioPids.empty())
				numpida = p_movie_info->audioPids.size();
			for (unsigned int i = 0; i < numpida; i++) {
				unsigned int j, asize = p_movie_info->audioPids.size();
				for (j = 0; j < asize && p_movie_info->audioPids[j].AudioPid != apids[i]; j++);
				if (j == asize) {
					AUDIO_PIDS pids;
					pids.AudioPid = apids[i];
					pids.selected = 0;
					pids.atype = ac3flags[i];
					pids.AudioPidName = language[i];
					p_movie_info->audioPids.push_back(pids);
				}
			}
		}else{
			for (unsigned int i = 0; i < numpida; i++)
				if (apids[i] == playback->GetAPid()) {
				CZapit::getInstance()->SetVolumePercent((ac3flags[i] == 1) ? g_settings.audio_volume_percent_ac3 : g_settings.audio_volume_percent_pcm);
				break;
			}
		}
		repeat_mode = (repeat_mode_enum) g_settings.movieplayer_repeat_on;
		playstate = CMoviePlayerGui::PLAY;
		CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, true);
#if HAVE_SPARK_HARDWARE
		CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
		CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, false);
#endif
		if (timeshift != TSHIFT_MODE_OFF) {
			startposition = -1;
			int i;
			int towait = (timeshift == TSHIFT_MODE_ON) ? TIMESHIFT_SECONDS+1 : TIMESHIFT_SECONDS;
			int cnt = 500;
			if (IS_WEBCHAN(movie_info.channelId)) {
				videoDecoder->setBlank(false);
				cnt = 200;
				towait = 20;
			}
			for(i = 0; i < cnt; i++) {
				playback->GetPosition(position, duration);
				startposition = (duration - position);

				//printf("CMoviePlayerGui::PlayFile: waiting for data, position %d duration %d (%d), start %d\n", position, duration, towait, startposition);
				if (startposition > towait*1000)
					break;

				usleep(20000);
			}
			printf("CMoviePlayerGui::PlayFile: waiting for data: i=%d position %d duration %d (%d), start %d\n", i, position, duration, towait, startposition);
			if (timeshift == TSHIFT_MODE_REWIND) {
				startposition = duration;
			} else {
				if (g_settings.timeshift_pause)
				{
					playstate = CMoviePlayerGui::PAUSE;
#if HAVE_SPARK_HARDWARE
					CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, false);
					CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
					CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
					CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, true);
#endif
				}
				if (timeshift == TSHIFT_MODE_ON)
					startposition = 0;
				else
					startposition = std::max(0, duration - towait*1000);
			}
			printf("******************* Timeshift %d, position %d, seek to %d seconds\n", timeshift, position, startposition/1000);
		}
		if (/* !is_file_player && */ startposition >= 0)
			playback->SetPosition(startposition, true);


		/* playback->Start() starts paused */
		if (timeshift == TSHIFT_MODE_REWIND) {
			speed = -1;
			playback->SetSpeed(-1);
			playstate = CMoviePlayerGui::REW;
			if (!FileTimeOSD->IsVisible() && !time_forced) {
				FileTimeOSD->switchMode(position, duration);
				time_forced = true;
			}
			FileTimeOSD->setMpTimeForced(true);
		} else if (timeshift == TSHIFT_MODE_OFF || !g_settings.timeshift_pause) {
			playback->SetSpeed(1);
		}
	}
	getCurrentAudioName(is_file_player, currentaudioname);
	if (is_file_player)
		selectAutoLang();

	enableOsdElements(MUTE);
	return res;
}

bool CMoviePlayerGui::SetPosition(int pos, bool absolute)
{
	StopSubtitles(true);
	bool res = playback->SetPosition(pos, absolute);
	if(is_file_player && res && speed == 0 && playstate == CMoviePlayerGui::PAUSE){
		playstate = CMoviePlayerGui::PLAY;
		speed = 1;
		playback->SetSpeed(speed);
	}
	StartSubtitles(true);
	return res;
}

void CMoviePlayerGui::quickZap(neutrino_msg_t msg)
{
	if ((msg == CRCInput::RC_right) || msg == (neutrino_msg_t) g_settings.key_quickzap_up)
	{
		//printf("CMoviePlayerGui::%s: CRCInput::RC_right or g_settings.key_quickzap_up\n", __func__);
		if (isLuaPlay || isUPNP)
		{
			playstate = CMoviePlayerGui::STOPPED;
			keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_NEXT;
			ClearQueue();
		}
		else if (!filelist.empty())
		{
			if (filelist_it < (filelist.end() - 1))
			{
				playstate = CMoviePlayerGui::STOPPED;
				++filelist_it;
			}
			else if (repeat_mode == REPEAT_ALL)
			{
				playstate = CMoviePlayerGui::STOPPED;
				++filelist_it;
				if (filelist_it == filelist.end())
				{
					filelist_it = filelist.begin();
				}
			}
		}
	}
	else if ((msg == CRCInput::RC_left) || msg == (neutrino_msg_t) g_settings.key_quickzap_down)
	{
		//printf("CMoviePlayerGui::%s: CRCInput::RC_left or g_settings.key_quickzap_down\n", __func__);
		if (isLuaPlay || isUPNP)
		{
			playstate = CMoviePlayerGui::STOPPED;
			keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_PREV;
			ClearQueue();
		}
		else if (filelist.size() > 1)
		{
			if (filelist_it != filelist.begin())
			{
				playstate = CMoviePlayerGui::STOPPED;
				--filelist_it;
			}
		}
	}
}

void CMoviePlayerGui::PlayFileLoop(void)
{
	bool first_start = true;
	bool update_lcd = true;
#if 0
	neutrino_msg_t lastmsg = 0;
#endif
	int ss,mm,hh;
#if HAVE_COOL_HARDWARE
	int eof = 0;
	int eof2 = 0;
	int position_tmp = 0;
#endif
	bool at_eof = !(playstate >= CMoviePlayerGui::PLAY);;
	keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_NORMAL;

#if 0	//bisectional jumps
	int bisection_jump = g_settings.movieplayer_bisection_jump * 60;
	int bisection_loop = -1;
	int bisection_loop_max = 5;
#endif
	while (playstate >= CMoviePlayerGui::PLAY)
	{
#ifdef ENABLE_GRAPHLCD
		if (p_movie_info)
			nGLCD::lockChannel(p_movie_info->channelName, p_movie_info->epgTitle, duration ? (100 * position / duration) : 0);
		else {
			glcd_play = true;
			nGLCD::lockChannel(g_Locale->getText(LOCALE_MOVIEPLAYER_HEAD), file_name.c_str(), file_prozent);
		}
#endif
		if (update_lcd) {
			update_lcd = false;
			updateLcd();
		}
		if (first_start) {
			callInfoViewer();
			first_start = false;
		}

		neutrino_msg_t msg;
		neutrino_msg_data_t data;
		g_RCInput->getMsg(&msg, &data, 10);	// 1 secs..

		// handle CRCInput::RC_playpause key
		bool handle_key_play = true;
		bool handle_key_pause = true;

#if 0		//bisectional jumps
		if (g_settings.mpkey_play == g_settings.mpkey_pause)
		{
			if (playstate == CMoviePlayerGui::PLAY)
				handle_key_play = false;
			else if (playstate == CMoviePlayerGui::PAUSE)
				handle_key_pause = false;
		}

		if (bisection_loop > -1)
			bisection_loop++;
		if (bisection_loop > bisection_loop_max)
			bisection_loop = -1;
#endif
		if ((playstate >= CMoviePlayerGui::PLAY) && (timeshift != TSHIFT_MODE_OFF || (playstate != CMoviePlayerGui::PAUSE))) {
			if (playback->GetPosition(position, duration)) {
				FileTimeOSD->update(position, duration);
				if (duration > 100)
					file_prozent = (unsigned char) (position / (duration / 100));

#if HAVE_TRIPLEDRAGON
				CVFD::getInstance()->showPercentOver(file_prozent, true, CVFD::MODE_MOVIE);
#else
				CVFD::getInstance()->showPercentOver(file_prozent);
#endif
				if (g_info.hw_caps->display_xres > 8)
				{
					ss = position/1000;
					hh = ss/3600;
					ss -= hh * 3600;
					mm = ss/60;
					ss -= mm * 60;
					std::string Value = to_string(hh/10) + to_string(hh%10) + ":" + to_string(mm/10) + to_string(mm%10) + ":" + to_string(ss/10) + to_string(ss%10);
					CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
					CVFD::getInstance()->showMenuText(0, Value.c_str(), -1, true);
				}

				playback->GetSpeed(speed);
				/* at BOF lib set speed 1, check it */
				if ((playstate != CMoviePlayerGui::PLAY) && (speed == 1)) {
					playstate = CMoviePlayerGui::PLAY;
					update_lcd = true;
				}
#ifdef DEBUG
				printf("CMoviePlayerGui::%s: spd %d pos %d/%d (%d, %d%%)\n", __func__, speed, position, duration, duration-position, file_prozent);
#endif
			} else
#if HAVE_COOL_HARDWARE
			{
				/* in case ffmpeg report incorrect values */
				if(file_prozent > 89 && (playstate == CMoviePlayerGui::PLAY) && (speed == 1)){
					if(position_tmp != position){
						position_tmp = position ;
						eof2 = 0;
					}else{
						if (++eof2 > 12) {
							at_eof = true;
							break;
						}
					}
				}
				else{
					eof2 = 0;
				}
				int posdiff = duration - position;
				if ((posdiff >= 0) && (posdiff < 2000) && timeshift == TSHIFT_MODE_OFF)
				{
					int delay = (filelist_it != filelist.end() || repeat_mode != REPEAT_OFF) ? 5 : 10;
					if (++eof > delay) {
						at_eof = true;
						break;
					}
				}
				else
					eof = 0;
			}
#else
			{
				if (filelist_it == filelist.end() - 1 || filelist_it == filelist.end())
					g_RCInput->postMsg((neutrino_msg_t) g_settings.mpkey_stop, 0);
				else
					g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_right, 0);
			}
#endif
			handleMovieBrowser(0, position);
			if (playstate == CMoviePlayerGui::STOPPED)
				at_eof = true;

			FileTimeOSD->update(position, duration);
		}
#if 0
		showSubtitle(0);
#endif

		if (msg <= CRCInput::RC_MaxRC)
			CScreenSaver::getInstance()->resetIdleTime();

		if (playstate == CMoviePlayerGui::PAUSE && (msg == CRCInput::RC_timeout || msg == NeutrinoMessages::EVT_TIMER))
		{
			if (CScreenSaver::getInstance()->canStart() && !CScreenSaver::getInstance()->isActive())
			{
				videoDecoder->setBlank(true);
				CScreenSaver::getInstance()->Start();
			}
		}
		else
		{
			if (CScreenSaver::getInstance()->isActive())
			{
				videoDecoder->setBlank(false);
				CScreenSaver::getInstance()->Stop();
				if (msg <= CRCInput::RC_MaxRC)
				{
					//ignore first keypress - just quit the screensaver and call infoviewer
					g_RCInput->clearRCMsg();
					callInfoViewer();
					continue;
				}

			}
		}

		if (msg == (neutrino_msg_t) g_settings.mpkey_plugin) {
			g_Plugins->startPlugin_by_name(g_settings.movieplayer_plugin.c_str ());
#if 0
		} else if ((msg == (neutrino_msg_t) g_settings.mpkey_stop) || msg == CRCInput::RC_home) {
#endif
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_stop) {
			playstate = CMoviePlayerGui::STOPPED;
			keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_STOP;
			ClearQueue();
		} else if ((!filelist.empty() && msg == (neutrino_msg_t) CRCInput::RC_ok)) {
			disableOsdElements(MUTE);
			CFileBrowser *playlist = new CFileBrowser();
			CFile *pfile = NULL;
			pfile = &(*filelist_it);
			int selected = std::distance( filelist.begin(), filelist_it );
			filelist_it = filelist.end();
			if (playlist->playlist_manager(filelist, selected, is_audio_player))
			{
				playstate = CMoviePlayerGui::STOPPED;
				CFile *sfile = NULL;
				for (filelist_it = filelist.begin(); filelist_it != filelist.end(); ++filelist_it)
				{
					pfile = &(*filelist_it);
					sfile = playlist->getSelectedFile();
					if ( (sfile->getFileName() == pfile->getFileName()) && (sfile->getPath() == pfile->getPath()))
						break;
				}
			}
			else {
				if (!filelist.empty())
					filelist_it = filelist.begin() + selected;
			}
			delete playlist;
			enableOsdElements(MUTE);
		} else if (msg == CRCInput::RC_left || msg == CRCInput::RC_right) {
			bool reset_vzap_it = true;
			switch (g_settings.mode_left_right_key_tv)
			{
				case SNeutrinoSettings::INFOBAR:
					callInfoViewer();
					break;
				case SNeutrinoSettings::VZAP:
					if (fromInfoviewer)
					{
						set_vzap_it(msg == CRCInput::RC_right);
						reset_vzap_it = false;
						fromInfoviewer = false;
					}
					callInfoViewer(reset_vzap_it);
					break;
				case SNeutrinoSettings::VOLUME:
					g_volume->setVolume(msg);
					break;
				default: /* SNeutrinoSettings::ZAP */
					quickZap(msg);
					break;
			}
		} else if (msg == (neutrino_msg_t) g_settings.key_quickzap_up || msg == (neutrino_msg_t) g_settings.key_quickzap_down) {
			quickZap(msg);
		} else if (fromInfoviewer && msg == CRCInput::RC_ok && !filelist.empty()) {
			printf("CMoviePlayerGui::%s: start playlist movie #%d\n", __func__, (int)(vzap_it - filelist.begin()));
			fromInfoviewer = false;
			playstate = CMoviePlayerGui::STOPPED;
			filelist_it = vzap_it;
		} else if (timeshift == TSHIFT_MODE_OFF && !isWebChannel /* && !isYT */ && (msg == (neutrino_msg_t) g_settings.mpkey_next_repeat_mode)) {
			repeat_mode = (repeat_mode_enum)((int)repeat_mode + 1);
			if (repeat_mode > (int) REPEAT_ALL)
				repeat_mode = REPEAT_OFF;
			g_settings.movieplayer_repeat_on = repeat_mode;
			callInfoViewer();
#if HAVE_SPARK_HARDWARE
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_next3dmode) {
			frameBuffer->set3DMode((CFrameBuffer::Mode3D)(((frameBuffer->get3DMode()) + 1) % CFrameBuffer::Mode3D_SIZE));
		} else if (msg == (neutrino_msg_t) g_settings.key_next43mode) {
			g_videoSettings->next43Mode();
		} else if (msg == (neutrino_msg_t) g_settings.key_switchformat) {
			g_videoSettings->SwitchFormat();
#endif
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_play && handle_key_play) {
			if (time_forced) {
				time_forced = false;
				FileTimeOSD->kill();
			}
			FileTimeOSD->setMpTimeForced(false);
			if (playstate > CMoviePlayerGui::PLAY) {
				playstate = CMoviePlayerGui::PLAY;
#if HAVE_SPARK_HARDWARE
				CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, true);
				CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
#endif
				speed = 1;
				playback->SetSpeed(speed);
				updateLcd();
				if (timeshift == TSHIFT_MODE_OFF)
					callInfoViewer();
			} else if (!filelist.empty()) {
				disableOsdElements(MUTE);
				CFileBrowser *playlist = new CFileBrowser();
				CFile *pfile = NULL;
				int selected = std::distance( filelist.begin(), filelist_it );
				filelist_it = filelist.end();
				if (playlist->playlist_manager(filelist, selected, is_audio_player))
				{
					playstate = CMoviePlayerGui::STOPPED;
					CFile *sfile = NULL;
					for (filelist_it = filelist.begin(); filelist_it != filelist.end(); ++filelist_it)
					{
						pfile = &(*filelist_it);
						sfile = playlist->getSelectedFile();
						if ( (sfile->getFileName() == pfile->getFileName()) && (sfile->getPath() == pfile->getPath()))
							break;
					}
				}
				else {
					if (!filelist.empty())
						filelist_it = filelist.begin() + selected;
				}
				delete playlist;
				enableOsdElements(MUTE);
			}
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_pause && handle_key_pause) {
			if (time_forced) {
				time_forced = false;
				FileTimeOSD->kill();
			}
			FileTimeOSD->setMpTimeForced(false);
			if (playstate == CMoviePlayerGui::PAUSE) {
				playstate = CMoviePlayerGui::PLAY;
				//CVFD::getInstance()->ShowIcon(VFD_ICON_PAUSE, false);
#if HAVE_SPARK_HARDWARE
				CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, true);
				CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
#endif
				speed = 1;
				playback->SetSpeed(speed);
			} else {
				playstate = CMoviePlayerGui::PAUSE;
				//CVFD::getInstance()->ShowIcon(VFD_ICON_PAUSE, true);
#if HAVE_SPARK_HARDWARE
				CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, true);
				CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
#endif
				speed = 0;
				playback->SetSpeed(speed);
			}
			updateLcd();

			if (timeshift == TSHIFT_MODE_OFF)
				callInfoViewer();
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_bookmark) {
			handleMovieBrowser((neutrino_msg_t) g_settings.mpkey_bookmark, position);
			update_lcd = true;
#if 0
			clearSubtitle();
#endif
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_audio) {
			selectAudioPid();
			update_lcd = true;
#if 0
			clearSubtitle();
#endif
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_subtitle) {
			selectAudioPid();
#if 0
			selectSubtitle();
			clearSubtitle();
#endif
			update_lcd = true;
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_time) {
			FileTimeOSD->switchMode(position, duration);
			time_forced = false;
			FileTimeOSD->setMpTimeForced(false);
		} else if (msg == (neutrino_msg_t) g_settings.mbkey_cover) {
			makeScreenShot(false, true);
		} else if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
			makeScreenShot();
		} else if ((msg == (neutrino_msg_t) g_settings.mpkey_rewind) ||
				(msg == (neutrino_msg_t) g_settings.mpkey_forward)) {
			int newspeed = 0;
			bool setSpeed = false;
			if (msg == (neutrino_msg_t) g_settings.mpkey_rewind) {
				newspeed = (speed >= 0) ? -1 : (speed - 1);
#if HAVE_SPARK_HARDWARE
				CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, true);
				CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FR, true);
				CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
#endif
			} else {
				newspeed = (speed <= 0) ? 2 : (speed + 1);
#if HAVE_SPARK_HARDWARE
				CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, true);
				CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
				CVFD::getInstance()->ShowIcon(FP_ICON_FF, true);
#endif
			}
			/* if paused, playback->SetSpeed() start slow motion */
			if (playback->SetSpeed(newspeed)) {
				printf("SetSpeed: update speed\n");
				speed = newspeed;
				if (playstate != CMoviePlayerGui::PAUSE)
					playstate = msg == (neutrino_msg_t) g_settings.mpkey_rewind ? CMoviePlayerGui::REW : CMoviePlayerGui::FF;
				updateLcd();
				setSpeed = true;
			}

			if (!FileTimeOSD->IsVisible() && !time_forced && setSpeed) {
				FileTimeOSD->switchMode(position, duration);
				time_forced = true;
			}
			FileTimeOSD->setMpTimeForced(true);
			if (timeshift == TSHIFT_MODE_OFF)
				callInfoViewer();
		} else if (msg == CRCInput::RC_1) {	// Jump Backward 1 minute
			SetPosition(-60 * 1000);
		} else if (msg == CRCInput::RC_3) {	// Jump Forward 1 minute
			SetPosition(60 * 1000);
		} else if (msg == CRCInput::RC_4) {	// Jump Backward 5 minutes
			SetPosition(-5 * 60 * 1000);
		} else if (msg == CRCInput::RC_6) {	// Jump Forward 5 minutes
			SetPosition(5 * 60 * 1000);
		} else if (msg == CRCInput::RC_7) {	// Jump Backward 10 minutes
			SetPosition(-10 * 60 * 1000);
		} else if (msg == CRCInput::RC_9) {	// Jump Forward 10 minutes
			SetPosition(10 * 60 * 1000);
		} else if (msg == CRCInput::RC_2) {	// goto start
			SetPosition(0, true);
		} else if (msg == CRCInput::RC_5) {	// goto middle
			SetPosition(duration/2, true);
		} else if (msg == CRCInput::RC_8) {	// goto end
			SetPosition(duration - 60 * 1000, true);
#if HAVE_COOL_HARDWARE
		} else if (msg == CRCInput::RC_page_up) {
			SetPosition(10 * 1000);
		} else if (msg == CRCInput::RC_page_down) {
			SetPosition(-10 * 1000);
#endif
#if 0
		//- bisectional jumps
		} else if (msg == CRCInput::RC_page_up || msg == CRCInput::RC_page_down) {
			int direction = (msg == CRCInput::RC_page_up) ? 1 : -1;
			int jump = 10;

			if (g_settings.movieplayer_bisection_jump)
			{
				if ((lastmsg == CRCInput::RC_page_up || lastmsg == CRCInput::RC_page_down) && (bisection_loop > -1 && bisection_loop <= bisection_loop_max))
					bisection_jump /= 2;
				else
					bisection_jump = g_settings.movieplayer_bisection_jump * 60;

				bisection_loop = 0;
				jump = bisection_jump;
			}

			SetPosition(direction*jump * 1000);
#endif
		} else if (msg == CRCInput::RC_0) {	// cancel bookmark jump
			handleMovieBrowser(CRCInput::RC_0, position);
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_goto) {
			bool cancel = true;
			playback->GetPosition(position, duration);
			ss = position/1000;
			hh = ss/3600;
			ss -= hh * 3600;
			mm = ss/60;
			ss -= mm * 60;
			std::string Value = to_string(hh/10) + to_string(hh%10) + ":" + to_string(mm/10) + to_string(mm%10) + ":" + to_string(ss/10) + to_string(ss%10);
			CTimeInput jumpTime (LOCALE_MPKEY_GOTO, &Value, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, NULL, &cancel);
			jumpTime.exec(NULL, "");
			jumpTime.hide();
			if (!cancel && (3 == sscanf(Value.c_str(), "%d:%d:%d", &hh, &mm, &ss)))
				SetPosition(1000 * (hh * 3600 + mm * 60 + ss), true);

		} else if (msg == CRCInput::RC_help) {
			disableOsdElements(NO_MUTE);
			showHelp();
			enableOsdElements(NO_MUTE);
		} else if (msg == CRCInput::RC_info) {
			if (fromInfoviewer) {
				disableOsdElements(NO_MUTE);
#ifdef ENABLE_LUA
				if (isLuaPlay && haveLuaInfoFunc) {
					int xres = 0, yres = 0, aspectRatio = 0, framerate = -1;
					if (!videoDecoder->getBlank()) {
						videoDecoder->getPictureInfo(xres, yres, framerate);
						if (yres == 1088)
							yres = 1080;
						aspectRatio = videoDecoder->getAspectRatio();
					}
					CLuaInstVideo::getInstance()->execLuaInfoFunc(luaState, xres, yres, aspectRatio, framerate);
				}
				else {
#endif
					g_EpgData->show_mp(p_movie_info,GetPosition(),GetDuration());
#ifdef ENABLE_LUA
				}
#endif
				fromInfoviewer = false;
				enableOsdElements(NO_MUTE);
			}
			else
				callInfoViewer();
			update_lcd = true;
#if 0
			clearSubtitle();
#endif
#if HAVE_SPARK_HARDWARE
		} else if (msg == CRCInput::RC_text) {
			int pid = playback->GetFirstTeletextPid();
			if (pid > -1) {
				playback->SetTeletextPid(0);
				StopSubtitles(true);
				if (g_settings.cacheTXT)
					tuxtxt_stop();
				playback->SetTeletextPid(pid);
				tuxtx_stop_subtitle();
				tuxtx_main(pid, 0, 2, true);
				tuxtxt_stop();
				playback->SetTeletextPid(0);
				if (currentttxsub != "") {
					CSubtitleChangeExec SubtitleChanger(playback);
					SubtitleChanger.exec(NULL, currentttxsub);
				}
				StartSubtitles(true);
				frameBuffer->paintBackground();
				//purge input queue
				do
					g_RCInput->getMsg(&msg, &data, 1);
				while (msg != CRCInput::RC_timeout);
			} else if (g_RemoteControl->current_PIDs.PIDs.vtxtpid) {
				StopSubtitles(true);
				// The playback stream doesn't come with teletext.
				tuxtx_main(g_RemoteControl->current_PIDs.PIDs.vtxtpid, 0, 2);
				frameBuffer->paintBackground();
				StartSubtitles(true);
				//purge input queue
				do
					g_RCInput->getMsg(&msg, &data, 1);
				while (msg != CRCInput::RC_timeout);
			}
#endif
		} else if (timeshift != TSHIFT_MODE_OFF && (msg == CRCInput::RC_text || msg == CRCInput::RC_epg || msg == NeutrinoMessages::SHOW_EPG)) {
			bool restore = FileTimeOSD->IsVisible();
			FileTimeOSD->kill();

			StopSubtitles(true);
			if (msg == CRCInput::RC_epg)
				g_EventList->exec(CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID(), CNeutrinoApp::getInstance()->channelList->getActiveChannelName());
			else if (msg == NeutrinoMessages::SHOW_EPG)
				g_EpgData->show(CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID());
			StartSubtitles(true);
			if (restore)
				FileTimeOSD->show(position);
#if 0
		} else if (msg == CRCInput::RC_red) {
			bool restore = FileTimeOSD->IsVisible();
			FileTimeOSD->kill();
			CStreamInfo2 streaminfo;
			streaminfo.exec(NULL, "");
			if (restore)
				FileTimeOSD->show(position);
			update_lcd = true;
#endif
		} else if (msg == NeutrinoMessages::SHOW_EPG) {
			handleMovieBrowser(NeutrinoMessages::SHOW_EPG, position);
		} else if (msg == NeutrinoMessages::EVT_SUBT_MESSAGE) {
#if 0
			showSubtitle(data);
#endif
		} else if (msg == NeutrinoMessages::ANNOUNCE_RECORD ||
				msg == NeutrinoMessages::RECORD_START) {
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else if (msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::LEAVE_ALL ||
				msg == NeutrinoMessages::SHUTDOWN ||
				((msg == NeutrinoMessages::SLEEPTIMER) && !data) ) {	// Exit for Record/Zapto Timers
			printf("CMoviePlayerGui::PlayFile: ZAPTO etc..\n");
			if (msg != NeutrinoMessages::ZAPTO)
				menu_ret = menu_return::RETURN_EXIT_ALL;

			playstate = CMoviePlayerGui::STOPPED;
			keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_LEAVE_ALL;
			ClearQueue();
			g_RCInput->postMsg(msg, data);
		} else if (msg == CRCInput::RC_timeout || msg == NeutrinoMessages::EVT_TIMER) {
			if (playstate == CMoviePlayerGui::PLAY && (position >= 300000 || (duration < 300000 && (position > (duration /2)))))
				makeScreenShot(true);
		} else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			//FIXME do nothing ?
		} else if (msg == (neutrino_msg_t) CRCInput::RC_setup) {
			CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::SHOW_MAINMENU, 0);
		} else if (msg == CRCInput::RC_red || msg == CRCInput::RC_green || msg == CRCInput::RC_yellow || msg == CRCInput::RC_blue ) {
			//maybe move FileTimeOSD->kill to Usermenu to simplify this call
			bool restore = FileTimeOSD->IsVisible();
			FileTimeOSD->kill();
			CNeutrinoApp::getInstance()->usermenu.showUserMenu(msg);
			if (restore)
				FileTimeOSD->show(position);
			update_lcd = true;
		} else {
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all) {
				printf("CMoviePlayerGui::PlayFile: neutrino handleMsg messages_return::cancel_all\n");
				menu_ret = menu_return::RETURN_EXIT_ALL;
				playstate = CMoviePlayerGui::STOPPED;
				keyPressed = CMoviePlayerGui::PLUGIN_PLAYSTATE_LEAVE_ALL;
				ClearQueue();
			}
			else if (msg <= CRCInput::RC_MaxRC) {
				update_lcd = true;
#if 0
				clearSubtitle();
#endif
			}
		}
#if 0
		if (msg < CRCInput::RC_MaxRC)
			lastmsg = msg;
#endif
	}
	printf("CMoviePlayerGui::PlayFile: exit, isMovieBrowser %d p_movie_info %p\n", isMovieBrowser, p_movie_info);
	playstate = CMoviePlayerGui::STOPPED;
	handleMovieBrowser((neutrino_msg_t) g_settings.mpkey_stop, position);
	if (position >= 300000 || (duration < 300000 && (position > (duration /2))))
		makeScreenShot(true);

	if (at_eof && !filelist.empty()) {
		if (filelist_it != filelist.end() && repeat_mode != REPEAT_TRACK)
			++filelist_it;

		if (filelist_it == filelist.end() && repeat_mode == REPEAT_ALL)
			filelist_it = filelist.begin();
	}
}

void CMoviePlayerGui::PlayFileEnd(bool restore)
{
	printf("%s: stopping, this %p thread %p\n", __func__, this, CMoviePlayerGui::bgPlayThread);fflush(stdout);
	if (filelist_it == filelist.end())
		FileTimeOSD->kill();
#if 0
	clearSubtitle();
#endif

	playback->SetSpeed(1);
	playback->Close();
#if HAVE_SPARK_HARDWARE
	frameBuffer->set3DMode(old3dmode);
	CScreenSetup cSS;
	cSS.showBorder(CZapit::getInstance()->GetCurrentChannelID());
#endif
#ifdef ENABLE_GRAPHLCD
	if (p_movie_info || glcd_play == true) {
		glcd_play = false;
		nGLCD::unlockChannel();
	}
#endif
	if (iso_file) {
		iso_file = false;
		if (umount2(ISO_MOUNT_POINT, MNT_FORCE))
			perror(ISO_MOUNT_POINT);
	}

	CVFD::getInstance()->ShowIcon(FP_ICON_PLAY, false);
	CVFD::getInstance()->ShowIcon(FP_ICON_PAUSE, false);
#if HAVE_SPARK_HARDWARE
	CVFD::getInstance()->ShowIcon(FP_ICON_FR, false);
	CVFD::getInstance()->ShowIcon(FP_ICON_FF, false);
#endif

	if (restore)
		restoreNeutrino();

	stopped = true;
	printf("%s: stopped\n", __func__);
	if (!filelist.empty() && filelist_it != filelist.end()) {
		pretty_name.clear();
		prepareFile(&(*filelist_it));
	}
}

void CMoviePlayerGui::set_vzap_it(bool up)
{
	//printf("CMoviePlayerGui::%s: vzap_it: %d count %s\n", __func__, (int)(vzap_it - filelist.begin()), up ? "up" : "down");
	if (up)
	{
		if (vzap_it < (filelist.end() - 1))
			++vzap_it;
	}
	else
	{
		if (vzap_it > filelist.begin())
			--vzap_it;
	}
	//printf("CMoviePlayerGui::%s: vzap_it: %d\n", __func__, (int)(vzap_it - filelist.begin()));
}

void CMoviePlayerGui::callInfoViewer(bool init_vzap_it)
{
	if (init_vzap_it)
	{
		//printf("CMoviePlayerGui::%s: init_vzap_it\n", __func__);
		vzap_it = filelist_it;
	}

	if (timeshift != TSHIFT_MODE_OFF) {
		g_InfoViewer->showTitle(CNeutrinoApp::getInstance()->channelList->getActiveChannel());
		return;
	}

	std::vector<std::string> keys, values;
	playback->GetMetadata(keys, values);
	size_t count = keys.size();
	if (count > 0) {
		CMovieInfo cmi;
		cmi.clearMovieInfo(&movie_info);
		for (size_t i = 0; i < count; i++) {
			std::string key = trim(keys[i]);
			if (movie_info.epgTitle.empty() && !strcasecmp("title", key.c_str())) {
				movie_info.epgTitle = isUTF8(values[i]) ? values[i] : convertLatin1UTF8(values[i]);
				CVFD::getInstance()->showServicename(movie_info.epgTitle.c_str());
				continue;
			}
			if (movie_info.channelName.empty() && !strcasecmp("artist", key.c_str())) {
				movie_info.channelName = isUTF8(values[i]) ? values[i] : convertLatin1UTF8(values[i]);
				continue;
			}
			if (movie_info.epgInfo1.empty() && !strcasecmp("album", key.c_str())) {
				movie_info.epgInfo1 = isUTF8(values[i]) ? values[i] : convertLatin1UTF8(values[i]);
				continue;
			}
		}
		if (!movie_info.channelName.empty() || !movie_info.epgTitle.empty())
			p_movie_info = &movie_info;
#ifdef ENABLE_GRAPHLCD
		if (p_movie_info)
			nGLCD::lockChannel(p_movie_info->channelName, p_movie_info->epgTitle);
#endif
	}

	if (p_movie_info) {

		if(duration == 0)
			UpdatePosition();

		MI_MOVIE_INFO *mi;
		mi = p_movie_info;
		if (!filelist.empty() && g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP)
		{
			if (vzap_it <= filelist.end()) {
				unsigned idx = vzap_it - filelist.begin();
				//printf("CMoviePlayerGui::%s: idx: %d\n", __func__, idx);
				if(milist.size() > idx)
					mi = milist[idx];
			}
		}

		std::string channelName = mi->channelName;
		if (channelName.empty())
			channelName = pretty_name;

		std::string channelTitle = mi->epgTitle;
		if (channelTitle.empty())
			channelTitle = pretty_name;

		CVFD::getInstance()->ShowText(channelTitle.c_str());

		g_InfoViewer->showMovieTitle(playstate, mi->epgId >>16, channelName, mi->epgTitle, mi->epgInfo1,
			duration, position, repeat_mode, init_vzap_it ? 0 /*IV_MODE_DEFAULT*/ : 1 /*IV_MODE_VIRTUAL_ZAP*/);
		unlink("/tmp/cover.jpg");
		return;
	}

	/* not moviebrowser => use the filename as title */
	CVFD::getInstance()->ShowText(pretty_name.c_str());
	g_InfoViewer->showMovieTitle(playstate, 0, pretty_name, info_1, info_2, duration, position, repeat_mode);
	unlink("/tmp/cover.jpg");
}

bool CMoviePlayerGui::getAudioName(int apid, std::string &apidtitle)
{
	if (p_movie_info == NULL)
	{
		numpida = 0;
		playback->FindAllPids(apids, ac3flags, &numpida, language);
		for (unsigned int count = 0; count < numpida; count++)
			if(apid == apids[count]){
					apidtitle = getISO639Description(language[count].c_str());
			return true;
			}
	}
	else
	{
		if (!isMovieBrowser)
		{
			numpida = 0;
			playback->FindAllPids(apids, ac3flags, &numpida, language);
			for (unsigned int count = 0; count < numpida; count++)
				if(apid == apids[count]){
						apidtitle = getISO639Description(language[count].c_str());
				return true;
				}
		}
		else
		{
			for (int i = 0; i < (int)p_movie_info->audioPids.size(); i++) {
				if (p_movie_info->audioPids[i].AudioPid == apid && !p_movie_info->audioPids[i].AudioPidName.empty()) {
					apidtitle = getISO639Description(p_movie_info->audioPids[i].AudioPidName.c_str());
					return true;
				}
			}
		}
	}
	return false;
}

void CMoviePlayerGui::addAudioFormat(int count, std::string &apidtitle, bool& enabled)
{
	enabled = true;
	switch(ac3flags[count])
	{
		case 1: /*AC3,EAC3*/
			if (apidtitle.find("AC3") == std::string::npos)
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
			if (apidtitle.find("DTS") == std::string::npos)
				apidtitle.append(" (DTS)");
#ifndef BOXMODEL_CS_HD2
			enabled = false;
#endif
			break;
		case 7: /*MLP*/
			apidtitle.append(" (MLP)");
			break;
		default:
			break;
	}
}

void CMoviePlayerGui::getCurrentAudioName(bool file_player, std::string &audioname)
{
	if (file_player) {
		numpida = 0;
		playback->FindAllPids(apids, ac3flags, &numpida, language);
	}
	if (numpida && !currentapid)
		currentapid = apids[0];
	for (unsigned int count = 0; count < numpida; count++)
		if(currentapid == apids[count]){
			if (getAudioName(apids[count], audioname))
				return;
			audioname = language[count];
			return;
		}
}

void CMoviePlayerGui::selectAudioPid()
{
	CAudioSelectMenuHandler APIDSelector;
	StopSubtitles(true);
	APIDSelector.exec(NULL, "-1");
	StartSubtitles(true);
#if 0
	CMenuWidget APIDSelector(LOCALE_APIDSELECTOR_HEAD, NEUTRINO_ICON_AUDIO);
	APIDSelector.addIntroItems();

	int select = -1;
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	if (is_file_player && !numpida) {
		playback->FindAllPids(apids, ac3flags, &numpida, language);
		if (numpida)
			currentapid = apids[0];
	}
	for (unsigned int count = 0; count < numpida; count++) {
		bool name_ok = false;
		bool enabled = true;
		bool defpid = currentapid ? (currentapid == apids[count]) : (count == 0);
		std::string apidtitle;

		if (!is_file_player) {
			name_ok = getAudioName(apids[count], apidtitle);
		}
		else if (!language[count].empty()) {
			apidtitle = language[count];
			name_ok = true;
		}
		if (!name_ok) {
			char apidnumber[20];
			sprintf(apidnumber, "Stream %d %X", count + 1, apids[count]);
			apidtitle = apidnumber;
		}
		addAudioFormat(count, apidtitle, enabled);
		if (defpid && !enabled && (count < numpida)) {
			currentapid = apids[count+1];
			defpid = false;
		}

		char cnt[5];
		sprintf(cnt, "%d", count);
		CMenuForwarder * item = new CMenuForwarder(apidtitle.c_str(), enabled, NULL, selector, cnt, CRCInput::convertDigitToKey(count + 1));
		APIDSelector.addItem(item, defpid);
	}

	int percent[numpida+1];
	if (p_movie_info && numpida <= p_movie_info->audioPids.size()) {
		APIDSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_VOLUME_ADJUST));

		CVolume::getInstance()->SetCurrentChannel(p_movie_info->channelId);
		CVolume::getInstance()->SetCurrentPid(currentapid);
		for (uint i=0; i < numpida; i++) {
			percent[i] = CZapit::getInstance()->GetPidVolume(p_movie_info->channelId, apids[i], ac3flags[i]);
			APIDSelector.addItem(new CMenuOptionNumberChooser(p_movie_info->audioPids[i].AudioPidName,
						&percent[i], currentapid == apids[i],
						0, 999, CVolume::getInstance()));
		}
	}

	APIDSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE));
	extern CAudioSetupNotifier     * audioSetupNotifier;
	APIDSelector.addItem( new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_OUT, &g_settings.analog_out,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT,
				true, audioSetupNotifier, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN) );

	APIDSelector.exec(NULL, "");
	delete selector;
	printf("CMoviePlayerGui::selectAudioPid: selected %d (%x) current %x\n", select, (select >= 0) ? apids[select] : -1, currentapid);
	if ((select >= 0) && (currentapid != apids[select])) {
		currentapid = apids[select];
		currentac3 = ac3flags[select];
		playback->SetAPid(currentapid, currentac3);
		getCurrentAudioName(is_file_player, currentaudioname);
		printf("[movieplayer] apid changed to %d type %d\n", currentapid, currentac3);
	}
#endif
}

void CMoviePlayerGui::handleMovieBrowser(neutrino_msg_t msg, int /*position*/)
{
	CMovieInfo cMovieInfo;	// funktions to save and load movie info

	static int jump_not_until = 0;	// any jump shall be avoided until this time (in seconds from moviestart)
	static MI_BOOKMARK new_bookmark;	// used for new movie info bookmarks created from the movieplayer

	std::string key_bookmark = CRCInput::getKeyName((neutrino_msg_t) g_settings.mpkey_bookmark);
	char txt[1024];

	//TODO: width and height could be more flexible
	static int width = frameBuffer->getScreenWidth() / 2;
	static int height = 65;

	static int x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	static int y = frameBuffer->getScreenY() + frameBuffer->getScreenHeight() - height - 20;

	static CBox boxposition(x, y, width, height);	// window position for the hint boxes

	static CTextBox endHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_MOVIEEND), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	static CTextBox comHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_JUMPFORWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	static CTextBox loopHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_JUMPBACKWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);

	snprintf(txt, sizeof(txt)-1, g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_NEWBOOK_BACKWARD), key_bookmark.c_str());
	static CTextBox newLoopHintBox(txt, NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);

	snprintf(txt, sizeof(txt)-1, g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_NEWBOOK_FORWARD), key_bookmark.c_str());
	static CTextBox newComHintBox(txt, NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);

	static bool showEndHintBox = false;	// flag to check whether the box shall be painted
	static bool showComHintBox = false;	// flag to check whether the box shall be painted
	static bool showLoopHintBox = false;	// flag to check whether the box shall be painted

	int play_sec = position / 1000;	// get current seconds from moviestart
	if (msg == CRCInput::RC_nokey) {
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
#if 0
	else if ((msg == (neutrino_msg_t) g_settings.mpkey_stop) || msg == CRCInput::RC_home) {
#endif
	else if (msg == (neutrino_msg_t) g_settings.mpkey_stop) {
		// if we have a movie information, try to save the stop position
		printf("CMoviePlayerGui::handleMovieBrowser: stop, isMovieBrowser %d p_movie_info %p\n", isMovieBrowser, p_movie_info);
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
	else if ((msg == 0) && isMovieBrowser && (playstate == CMoviePlayerGui::PLAY) && p_movie_info) {
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
#define BOOKMARK_START_MENU_MAX_ITEMS 7
			CSelectedMenu cSelectedMenuBookStart[BOOKMARK_START_MENU_MAX_ITEMS];

			CMenuWidget bookStartMenu(LOCALE_MOVIEBROWSER_MENU_MAIN_BOOKMARKS, NEUTRINO_ICON_STREAMING);
			bookStartMenu.addIntroItems();
#if 0 // not supported, TODO
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_HEAD, !isMovieBrowser, NULL, &cSelectedMenuBookStart[0]));
			bookStartMenu.addItem(GenericMenuSeparatorLine);
#endif
			const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
			char play_pos[32];
			int lastplaystop = p_movie_info ? p_movie_info->bookmarks.lastPlayStop/60:0;
			snprintf(play_pos, sizeof(play_pos), "%3d %s", lastplaystop, unit_short_minute);
			char start_pos[32] = {0};
			if (p_movie_info && p_movie_info->bookmarks.start != 0)
				snprintf(start_pos, sizeof(start_pos), "%3d %s", p_movie_info->bookmarks.start/60, unit_short_minute);
			char end_pos[32] = {0};
			if (p_movie_info && p_movie_info->bookmarks.end != 0)
				snprintf(end_pos, sizeof(end_pos), "%3d %s", p_movie_info->bookmarks.end/60, unit_short_minute);

			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_LASTMOVIESTOP, isMovieBrowser, play_pos, &cSelectedMenuBookStart[1]));
			bookStartMenu.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_BOOK_ADD));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_NEW, isMovieBrowser, NULL, &cSelectedMenuBookStart[2]));
			bookStartMenu.addItem(new CMenuSeparator());
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE_FORWARD, isMovieBrowser, NULL, &cSelectedMenuBookStart[3], NULL, CRCInput::RC_red));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE_BACKWARD, isMovieBrowser, NULL, &cSelectedMenuBookStart[4], NULL, CRCInput::RC_green));
			bookStartMenu.addItem(new CMenuSeparator());
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART, isMovieBrowser, start_pos, &cSelectedMenuBookStart[5], NULL, CRCInput::RC_yellow));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIEEND, isMovieBrowser, end_pos, &cSelectedMenuBookStart[6], NULL, CRCInput::RC_blue));

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
					bookmarkmanager->createBookmark(file_name, bookmarktime);
				} else {
					fprintf(stderr, "too many bookmarks\n");
					DisplayErrorMessage(g_Locale->getText(LOCALE_MOVIEPLAYER_TOOMANYBOOKMARKS));	// UTF-8
				}
				cSelectedMenuBookStart[0].selected = false;	// clear for next bookmark menu
			} else
#endif
			if (cSelectedMenuBookStart[1].selected == true) {
				int pos = p_movie_info->bookmarks.lastPlayStop;
				printf("[mb] last play stop: %d\n", pos);
				SetPosition(pos*1000, true);
			} else if (cSelectedMenuBookStart[2].selected == true) {
				/* Moviebrowser plain bookmark */
				new_bookmark.pos = play_sec;
				new_bookmark.length = 0;
				if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true)
					cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
				new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
			} else if (cSelectedMenuBookStart[3].selected == true) {
				/* Moviebrowser jump forward bookmark */
				new_bookmark.pos = play_sec;
				TRACE("[mp] new bookmark 1. pos: %d\r\n", new_bookmark.pos);
				newComHintBox.paint();
			} else if (cSelectedMenuBookStart[4].selected == true) {
				/* Moviebrowser jump backward bookmark */
				new_bookmark.pos = play_sec;
				TRACE("[mp] new bookmark 1. pos: %d\r\n", new_bookmark.pos);
				newLoopHintBox.paint();
			} else if (cSelectedMenuBookStart[5].selected == true) {
				/* Moviebrowser movie start bookmark */
				p_movie_info->bookmarks.start = play_sec;
				TRACE("[mp] New movie start pos: %d\r\n", p_movie_info->bookmarks.start);
				cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
			} else if (cSelectedMenuBookStart[6].selected == true) {
				/* Moviebrowser movie end bookmark */
				p_movie_info->bookmarks.end = play_sec;
				TRACE("[mp]  New movie end pos: %d\r\n", p_movie_info->bookmarks.end);
				cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
			}
		}
	} else if (msg == NeutrinoMessages::SHOW_EPG && p_movie_info) {
		disableOsdElements(NO_MUTE);
		g_EpgData->show_mp(p_movie_info, position, duration);
		enableOsdElements(NO_MUTE);
	}
	return;
}

void CMoviePlayerGui::UpdatePosition()
{
	if (playback->GetPosition(position, duration)) {
		if (duration > 100)
			file_prozent = (unsigned char) (position / (duration / 100));
		FileTimeOSD->update(position, duration);
#ifdef DEBUG
		printf("CMoviePlayerGui::%s: spd %d pos %d/%d (%d, %d%%)\n", __func__, speed, position, duration, duration-position, file_prozent);
#endif
	}
}

void CMoviePlayerGui::StopSubtitles(bool enable_glcd_mirroring __attribute__((unused)))
{
#if HAVE_SPARK_HARDWARE
	printf("[CMoviePlayerGui] %s\n", __FUNCTION__);
	int ttx, ttxpid, ttxpage;

	int current_sub = playback->GetSubtitlePid();
	if (current_sub > -1)
		dvbsub_pause();
	tuxtx_subtitle_running(&ttxpid, &ttxpage, &ttx);
	if (ttx) {
		tuxtx_pause_subtitle(true);
		frameBuffer->paintBackground();
	}
#ifdef ENABLE_GRAPHLCD
	if (enable_glcd_mirroring)
		nGLCD::MirrorOSD(g_settings.glcd_mirror_osd);
#endif
#endif
}

void CMoviePlayerGui::showHelp()
{
	Helpbox helpbox(g_Locale->getText(LOCALE_MESSAGEBOX_INFO));
	helpbox.addSeparator();
	helpbox.addLine(NEUTRINO_ICON_BUTTON_PAUSE, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_PAUSE));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_FORWARD, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_FORWARD));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_BACKWARD, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_BACKWARD));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_STOP, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_STOP));
	helpbox.addSeparatorLine();
	helpbox.addLine(NEUTRINO_ICON_BUTTON_1, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_1));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_2, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_2));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_3, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_3));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_4, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_4));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_5, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_5));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_6, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_6));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_7, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_7));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_8, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_8));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_9, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_9));
	helpbox.addSeparatorLine();
	helpbox.addLine(NEUTRINO_ICON_BUTTON_MENU, g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_BUTTON_MENU));
	helpbox.addSeparator();
	helpbox.addLine(g_Locale->getText(LOCALE_MOVIEPLAYER_HELP_ADDITIONAL));

	helpbox.addExitKey(CRCInput::RC_ok);

	helpbox.show();
	helpbox.exec();
	helpbox.hide();
}

void CMoviePlayerGui::StartSubtitles(bool show __attribute__((unused)))
{
#if HAVE_SPARK_HARDWARE
	printf("[CMoviePlayerGui] %s: %s\n", __FUNCTION__, show ? "Show" : "Not show");
#ifdef ENABLE_GRAPHLCD
	nGLCD::MirrorOSD(false);
#endif

	if(!show)
		return;
	int current_sub = playback->GetSubtitlePid();
	if (current_sub > -1)
		dvbsub_start(current_sub, true);
	tuxtx_pause_subtitle(false);
#endif
}

#if 0
void CMoviePlayerGui::selectChapter()
{
	if (!is_file_player)
		return;

	std::vector<int> positions; std::vector<std::string> titles;
	playback->GetChapters(positions, titles);

	std::vector<int> playlists; std::vector<std::string> ptitles;
	int current = 0;
	playback->GetTitles(playlists, ptitles, current);

	if (positions.empty() && playlists.empty())
		return;

	CMenuWidget ChSelector(LOCALE_MOVIEBROWSER_MENU_MAIN_BOOKMARKS, NEUTRINO_ICON_AUDIO);
	//ChSelector.addIntroItems();
	ChSelector.addItem(GenericMenuCancel);

	int pselect = -1;
	CMenuSelectorTarget * pselector = new CMenuSelectorTarget(&pselect);

	int select = -1;
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	char cnt[5];
	if (!playlists.empty()) {
		ChSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEPLAYER_TITLES));
		for (unsigned i = 0; i < playlists.size(); i++) {
			sprintf(cnt, "%d", i);
			CMenuForwarder * item = new CMenuForwarder(ptitles[i].c_str(), current != playlists[i], NULL, pselector, cnt);
			ChSelector.addItem(item);
		}
	}

	if (!positions.empty()) {
		ChSelector.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEPLAYER_CHAPTERS));
		for (unsigned i = 0; i < positions.size(); i++) {
			sprintf(cnt, "%d", i);
			CMenuForwarder * item = new CMenuForwarder(titles[i].c_str(), true, NULL, selector, cnt, CRCInput::convertDigitToKey(i + 1));
			ChSelector.addItem(item, position > positions[i]);
		}
	}
	ChSelector.exec(NULL, "");
	delete selector;
	delete pselector;
	printf("CMoviePlayerGui::selectChapter: selected %d (%d)\n", select, (select >= 0) ? positions[select] : -1);
	printf("CMoviePlayerGui::selectChapter: pselected %d (%d)\n", pselect, (pselect >= 0) ? playlists[pselect] : -1);
	if (select >= 0) {
		playback->SetPosition(positions[select], true);
	} else if (pselect >= 0) {
		numsubs = numpida = 0;
		currentspid = -1;
		currentapid = 0;
		playback->SetTitle(playlists[pselect]);
	}
}

void CMoviePlayerGui::selectSubtitle()
{
	if (!is_file_player)
		return;

	CMenuWidget APIDSelector(LOCALE_SUBTITLES_HEAD, NEUTRINO_ICON_AUDIO);
	APIDSelector.addIntroItems();

	int select = -1;
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
	if (!numsubs)
		playback->FindAllSubs(spids, sub_supported, &numsubs, slanguage);

	CMenuOptionStringChooser * sc = new CMenuOptionStringChooser(LOCALE_SUBTITLES_CHARSET, &g_settings.subs_charset, currentspid == -1, NULL, CRCInput::RC_red, NULL, true);
	sc->addOption("UTF-8");
	sc->addOption("UCS-2");
	sc->addOption("CP1250");
	sc->addOption("CP1251");
	sc->addOption("CP1252");
	sc->addOption("CP1253");
	sc->addOption("KOI8-R");

	APIDSelector.addItem(sc);
	APIDSelector.addItem(GenericMenuSeparatorLine);

	char cnt[5];
	unsigned int count;
	for (count = 0; count < numsubs; count++) {
		bool enabled = sub_supported[count] && (currentspid != spids[count]);

		std::string title = slanguage[count];
		if (title.empty()) {
			char pidnumber[20];
			sprintf(pidnumber, "Stream %d %X", count + 1, spids[count]);
			title = pidnumber;
		}
		sprintf(cnt, "%d", count);
		CMenuForwarder * item = new CMenuForwarder(title.c_str(), enabled, NULL, selector, cnt, CRCInput::convertDigitToKey(count + 1));
		APIDSelector.addItem(item);
	}
	sprintf(cnt, "%d", count);
	APIDSelector.addItem(new CMenuForwarder(LOCALE_SUBTITLES_STOP, currentspid != -1, NULL, selector, cnt, CRCInput::RC_stop), currentspid > 0);

	APIDSelector.exec(NULL, "");
	delete selector;
	printf("CMoviePlayerGui::selectSubtitle: selected %d (%x) current %x\n", select, (select >= 0) ? spids[select] : -1, currentspid);
	if ((select >= 0) && (select < numsubs) && (currentspid != spids[select])) {
		currentspid = spids[select];
		/* external subtitles pid is 0x1FFF */
		ext_subs = (currentspid == 0x1FFF);
		playback->SelectSubtitles(currentspid, g_settings.subs_charset);
		printf("[movieplayer] spid changed to %d\n", currentspid);
	} else if (select > 0) {
		ext_subs = false;
		currentspid = -1;
		playback->SelectSubtitles(currentspid, g_settings.subs_charset);
		printf("[movieplayer] spid changed to %d\n", currentspid);
	}
}

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

void CMoviePlayerGui::clearSubtitle(bool lock)
{
	if ((max_x-min_x > 0) && (max_y-min_y > 0))
		frameBuffer->paintBackgroundBoxRel(min_x, min_y, max_x-min_x, max_y-min_y);

	min_x = CFrameBuffer::getInstance()->getScreenWidth();
	min_y = CFrameBuffer::getInstance()->getScreenHeight();
	max_x = CFrameBuffer::getInstance()->getScreenX();
	max_y = CFrameBuffer::getInstance()->getScreenY();
	end_time = 0;
	lock_subs = lock;
}

fb_pixel_t * simple_resize32(uint8_t * orgin, uint32_t * colors, int nb_colors, int ox, int oy, int dx, int dy);

bool CMoviePlayerGui::convertSubtitle(std::string &text)
{
	bool ret = false;
	iconv_t cd = iconv_open("UTF-8", g_settings.subs_charset.c_str());
	if (cd == (iconv_t)-1) {
		perror("iconv_open");
		return ret;
	}
	size_t ilen = text.length();
	size_t olen = ilen*4;
	size_t len = olen;
	char * buf = (char *) calloc(olen + 1, 1);
	if (buf == NULL) {
		iconv_close(cd);
		return ret;
	}
	char * out = buf;
	char * in = (char *) text.c_str();
	if (iconv(cd, &in, &ilen, &out, &olen) == (size_t)-1) {
		printf("CMoviePlayerGui::convertSubtitle: iconv error\n");
	}
	else {
		memset(buf + (len - olen), 0, olen);
		text = buf;
	}

	free(buf);
	iconv_close(cd);
	return true;
}

void CMoviePlayerGui::showSubtitle(neutrino_msg_data_t data)
{
	AVSubtitle * sub = (AVSubtitle *) data;
	if (lock_subs) {
		if (sub) {
			avsubtitle_free(sub);
			delete sub;
		}
		return;
	}

	if (!data) {
		if (end_time && time_monotonic_ms() > end_time) {
			printf("************************* hide subs *************************\n");
			clearSubtitle();
		}
		return;
	}

	printf("************************* EVT_SUBT_MESSAGE: num_rects %d fmt %d *************************\n",  sub->num_rects, sub->format);
#if 0
	if (!sub->num_rects)
		return;
#endif
	if (sub->format == 0) {
		int xres = 0, yres = 0, framerate;
		videoDecoder->getPictureInfo(xres, yres, framerate);

		double xc, yc;
		if (sub->num_rects && (sub->rects[0]->x + sub->rects[0]->w) <= 720 &&
				sub->rects[0]->y + sub->rects[0]->h <= 576) {
			xc = (double) CFrameBuffer::getInstance()->getScreenWidth(/*true*/)/(double) 720;
			yc = (double) CFrameBuffer::getInstance()->getScreenHeight(/*true*/)/(double) 576;
		} else {
			xc = (double) CFrameBuffer::getInstance()->getScreenWidth(/*true*/)/(double) xres;
			yc = (double) CFrameBuffer::getInstance()->getScreenHeight(/*true*/)/(double) yres;
		}

		clearSubtitle();

		for (unsigned i = 0; i < sub->num_rects; i++) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
			uint32_t * colors = (uint32_t *) sub->rects[i]->pict.data[1];
#else
			uint32_t * colors = (uint32_t *) sub->rects[i]->data[1];
#endif
			int xoff = (double) sub->rects[i]->x * xc;
			int yoff = (double) sub->rects[i]->y * yc;
			int nw = frameBuffer->getWidth4FB_HW_ACC(xoff, (double) sub->rects[i]->w * xc);
			int nh = (double) sub->rects[i]->h * yc;

			printf("Draw: #%d at %d,%d size %dx%d colors %d (x=%d y=%d w=%d h=%d) \n", i+1,
					sub->rects[i]->x, sub->rects[i]->y, sub->rects[i]->w, sub->rects[i]->h,
					sub->rects[i]->nb_colors, xoff, yoff, nw, nh);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
			fb_pixel_t * newdata = simple_resize32 (sub->rects[i]->pict.data[0], colors,
					sub->rects[i]->nb_colors, sub->rects[i]->w, sub->rects[i]->h, nw, nh);
#else
			fb_pixel_t * newdata = simple_resize32 (sub->rects[i]->data[0], colors,
					sub->rects[i]->nb_colors, sub->rects[i]->w, sub->rects[i]->h, nw, nh);
#endif

			frameBuffer->blit2FB(newdata, nw, nh, xoff, yoff);

			min_x = std::min(min_x, xoff);
			max_x = std::max(max_x, xoff + nw);
			min_y = std::min(min_y, yoff);
			max_y = std::max(max_y, yoff + nh);
		}

		//end_time = sub->end_display_time + time_monotonic_ms();
		end_time = 10*1000;
		if (sub->end_display_time != UINT32_MAX)
			end_time = sub->end_display_time;
		end_time += time_monotonic_ms();

		avsubtitle_free(sub);
		delete sub;
		return;
	}
	std::vector<std::string> subtext;
	for (unsigned i = 0; i < sub->num_rects; i++) {
		char * txt = NULL;
		if (sub->rects[i]->type == SUBTITLE_TEXT)
			txt = sub->rects[i]->text;
		else if (sub->rects[i]->type == SUBTITLE_ASS)
			txt = sub->rects[i]->ass;
		printf("subt[%d] type %d [%s]\n", i, sub->rects[i]->type, txt ? txt : "");
		if (txt) {
			int len = strlen(txt);
			if (len > 10 && memcmp(txt, "Dialogue: ", 10) == 0) {
				char* p = txt;
#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(55, 69, 100)
				int skip_commas = 4;
#else
				int skip_commas = 9;
#endif
				/* skip ass times */
				for (int j = 0; j < skip_commas && *p != '\0'; p++)
					if (*p == ',')
						j++;
				/* skip ass tags */
				if (*p == '{') {
					char * d = strchr(p, '}');
					if (d)
						p += d - p + 1;
				}
				char * d = strchr(p, '{');
				if (d && strchr(d, '}'))
					*d = 0;

				len = strlen(p);
				/* remove newline */
				for (int j = len-1; j > 0; j--) {
					if (p[j] == '\n' || p[j] == '\r')
						p[j] = 0;
					else
						break;
				}
				if (*p == '\0')
					continue;
				txt = p;
			}
			//printf("title: [%s]\n", txt);
			std::string str(txt);
			size_t start = 0, end = 0;
			/* split string with \N as newline */
			std::string delim("\\N");
			while ((end = str.find(delim, start)) != string::npos) {
				subtext.push_back(str.substr(start, end - start));
				start = end + 2;
			}
			subtext.push_back(str.substr(start));
		}
	}
	for (unsigned i = 0; i < subtext.size(); i++) {
		if (!isUTF8(subtext[i])) {
			if (g_settings.subs_charset != "UTF-8")
				convertSubtitle(subtext[i]);
			else
				subtext[i] = convertLatin1UTF8(subtext[i]);
		}
		printf("subtext %d: [%s]\n", i, subtext[i].c_str());
	}
	printf("********************************************************************\n");

	if (!subtext.empty()) {
		int sh = frameBuffer->getScreenHeight();
		int sw = frameBuffer->getScreenWidth();
		int h = g_Font[SNeutrinoSettings::FONT_TYPE_SUBTITLES]->getHeight();
		int height = h*subtext.size();

		clearSubtitle();

		int x[subtext.size()];
		int y[subtext.size()];
		for (unsigned i = 0; i < subtext.size(); i++) {
			int w = g_Font[SNeutrinoSettings::FONT_TYPE_SUBTITLES]->getRenderWidth (subtext[i]);
			x[i] = (sw - w) / 2;
			y[i] = sh - height + h*(i + 1);
			min_x = std::min(min_x, x[i]);
			max_x = std::max(max_x, x[i]+w);
			min_y = std::min(min_y, y[i]-h);
			max_y = std::max(max_y, y[i]);
		}

		frameBuffer->paintBoxRel(min_x, min_y, max_x - min_x, max_y-min_y, COL_MENUCONTENT_PLUS_0);

		for (unsigned i = 0; i < subtext.size(); i++)
			g_Font[SNeutrinoSettings::FONT_TYPE_SUBTITLES]->RenderString(x[i], y[i], sw, subtext[i].c_str(), COL_MENUCONTENT_TEXT);

		end_time = sub->end_display_time + time_monotonic_ms();
	}
	avsubtitle_free(sub);
	delete sub;
}
#endif

bool CMoviePlayerGui::setAPID(unsigned int i) {
	if (currentapid != apids[i]) {
		currentapid = apids[i];
		currentac3 = ac3flags[i];
		playback->SetAPid(currentapid, currentac3);
		CZapit::getInstance()->SetVolumePercent((ac3flags[i] == 1) ? g_settings.audio_volume_percent_ac3 : g_settings.audio_volume_percent_pcm);
	}
	return (i < numpida);
}

std::string CMoviePlayerGui::getAPIDDesc(unsigned int i)
{
	std::string apidtitle;
	if (i < numpida)
		getAudioName(apids[i], apidtitle);
	if (apidtitle == "")
		apidtitle = "Stream " + to_string(i);
	return apidtitle;
}

unsigned int CMoviePlayerGui::getAPID(unsigned int i)
{
	if (i < numpida)
		return apids[i];
	return -1;
}

unsigned int CMoviePlayerGui::getAPID(void)
{
	for (unsigned int i = 0; i < numpida; i++)
		if (apids[i] == currentapid)
			return i;
	return -1;
}

unsigned int CMoviePlayerGui::getAPIDCount(void)
{
	if (!is_file_player && numpida)
		return numpida;

	unsigned int count = 0;
	numpida = 0;
	playback->FindAllPids(apids, ac3flags, &numpida, language);
	for (unsigned int i = 0; i < numpida; i++) {
		if (i != count) {
			apids[count] = apids[i];
			ac3flags[count] = ac3flags[i];
			language[count] = language[i];
		}
		if (language[i].empty()) {
			language[i] = "Stream ";
			language[i] += to_string(count);
		}
		bool ena = false;
		addAudioFormat(i, language[i], ena);
		if (ena)
			count++;
	}
	numpida = count;
	return numpida;
}

unsigned int CMoviePlayerGui::getSubtitleCount(void)
{
	// these may change in-stream
	numpids = REC_MAX_SPIDS;
	playback->FindAllSubtitlePids(spids, &numpids, slanguage);
	numpidt = REC_MAX_TPIDS;
	playback->FindAllTeletextsubtitlePids(tpids, &numpidt, tlanguage, tmag, tpage);

	return numpids + numpidt;
}

CZapitAbsSub* CMoviePlayerGui::getChannelSub(unsigned int i, CZapitAbsSub **s)
{
	if (i < numpidt) {
		CZapitTTXSub *_s = new CZapitTTXSub;
		_s->thisSubType = CZapitAbsSub::TTX;
		_s->pId = tpids[i];
		_s->ISO639_language_code = tlanguage[i];
		_s->teletext_magazine_number = tmag[i];
		_s->teletext_page_number = tpage[i];
		*s = _s;
		return *s;
	}
	i -= numpidt;
	if (i < numpids) {
		CZapitAbsSub *_s = new CZapitAbsSub;
		_s->thisSubType = CZapitAbsSub::SUB;
		_s->pId = spids[i];
		_s->ISO639_language_code = slanguage[i];
		*s = _s;
		return *s;
	}
	return NULL;
}

int CMoviePlayerGui::getCurrentSubPid(CZapitAbsSub::ZapitSubtitleType st)
{
	switch(st) {
		case CZapitAbsSub::DVB:
		case CZapitAbsSub::SUB:
			return playback->GetSubtitlePid();
		case CZapitAbsSub::TTX:
			return -1; // FIXME ... caller would need both pid and page
	}
	return -1;
}

t_channel_id CMoviePlayerGui::getChannelId(void)
{
	return p_movie_info ? p_movie_info->epgId : 0;
}

void CMoviePlayerGui::getAPID(int &apid, unsigned int &is_ac3)
{
	apid = currentapid, is_ac3 = (currentac3 == AUDIO_FMT_DOLBY_DIGITAL || currentac3 == AUDIO_FMT_DD_PLUS);
}

bool CMoviePlayerGui::getAPID(unsigned int i, int &apid, unsigned int &is_ac3)
{
	if (i < numpida) {
		apid = apids[i];
		is_ac3 = (ac3flags[i] == 1);
		return true;
	}
	return false;
}

void CMoviePlayerGui::selectAutoLang()
{
#if 0
	if (!numsubs)
		playback->FindAllSubs(spids, sub_supported, &numsubs, slanguage);

	if (ext_subs) {
		for (unsigned count = 0; count < numsubs; count++) {
			if (spids[count] == 0x1FFF) {
				currentspid = spids[count];
				playback->SelectSubtitles(currentspid, g_settings.subs_charset);
			}
		}
	}
#endif
	if (g_settings.auto_lang &&  (numpida > 1)) {
		int pref_idx = -1;

		playback->FindAllPids(apids, ac3flags, &numpida, language);
		for (int i = 0; i < 3; i++) {
			for (unsigned j = 0; j < numpida; j++) {
				std::map<std::string, std::string>::const_iterator it;
				for (it = iso639.begin(); it != iso639.end(); ++it) {
					if (g_settings.pref_lang[i] == it->second && strncasecmp(language[j].c_str(), it->first.c_str(), 3) == 0) {
						bool enabled = true;
						// TODO: better check of supported
						std::string audioname;
						addAudioFormat(j, audioname, enabled);
						if (enabled)
							pref_idx = j;
						break;
					}
				}
				if (pref_idx >= 0)
					break;
			}
			if (pref_idx >= 0)
				break;
		}
		if (pref_idx >= 0) {
			currentapid = apids[pref_idx];
			currentac3 = ac3flags[pref_idx];
			playback->SetAPid(currentapid, currentac3);
			getCurrentAudioName(is_file_player, currentaudioname);
		}
	}
#if 0
	if (isWebChannel && g_settings.auto_subs && numsubs > 0) {
		for(int i = 0; i < 3; i++) {
			if(g_settings.pref_subs[i].empty() || g_settings.pref_subs[i] == "none")
				continue;

			std::string temp(g_settings.pref_subs[i]);
			std::string slang;
			for (int j = 0 ; j < numsubs; j++) {
				if (!sub_supported[j])
					continue;
				slang = slanguage[j].substr(0, 3);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); ++it) {
					if(temp == it->second && slang == it->first) {
						currentspid = spids[j];
						break;
					}
				}
				if (currentspid > 0)
					break;
			}
			if (currentspid > 0) {
				playback->SelectSubtitles(currentspid, g_settings.subs_charset);
				printf("[movieplayer] spid changed to %d %s (%s)\n", currentspid, temp.c_str(), slang.c_str());
				break;
			}
		}
	}
#endif
}

void CMoviePlayerGui::parsePlaylist(CFile *file)
{
	std::ifstream infile;
	char cLine[1024];
	char name[1024] = { 0 };
	std::string file_path = file->getPath();
	infile.open(file->Name.c_str(), std::ifstream::in);
	filelist_it = filelist.erase(filelist_it);
	CFile tmp_file;
	while (infile.good())
	{
		infile.getline(cLine, sizeof(cLine));
		size_t len = strlen(cLine);
		if (len > 0 && cLine[len-1]=='\r')
			cLine[len-1]=0;

		int dur;
		sscanf(cLine, "#EXTINF:%d,%[^\n]\n", &dur, name);
		if (len > 0 && cLine[0]!='#')
		{
			char *url = NULL;
			if ((url = strstr(cLine, "http://")) || (url = strstr(cLine, "https://")) || (url = strstr(cLine, "rtmp://")) || (url = strstr(cLine, "rtsp://")) || (url = strstr(cLine, "mmsh://")) ) {
				if (url != NULL) {
					printf("name %s [%d] url: %s\n", name, dur, url);
					tmp_file.Name = name;
					tmp_file.Url = url;
					filelist.push_back(tmp_file);
				}
			}
			else
			{
				printf("name %s [%d] file: %s\n", name, dur, cLine);
				std::string illegalChars = "\\/:?\"<>|";
				std::string::iterator it;
				std::string name_s = name;
				for (it = name_s.begin() ; it < name_s.end() ; ++it){
					bool found = illegalChars.find(*it) != std::string::npos;
					if(found){
						*it = ' ';
					}
				}
				tmp_file.Name = name_s;
				tmp_file.Url = file_path + cLine;
				filelist.push_back(tmp_file);
			}
		}
	}
	filelist_it = filelist.begin();
}

bool CMoviePlayerGui::mountIso(CFile *file)
{
	printf("ISO file passed: %s\n", file->Name.c_str());
	safe_mkdir(ISO_MOUNT_POINT);
	if (my_system(5, "mount", "-o", "loop", file->Name.c_str(), ISO_MOUNT_POINT) == 0) {
		makeFilename();
		file_name = "/media/iso";
		iso_file = true;
		return true;
	}
	return false;
}

void CMoviePlayerGui::makeScreenShot(bool autoshot, bool forcover)
{
	if (autoshot && (autoshot_done || !g_settings.auto_cover))
		return;

#ifndef SCREENSHOT
	(void)forcover; // avoid compiler warning
#else
	bool cover = autoshot || g_settings.screenshot_cover || forcover;
	char ending[(sizeof(int)*2) + 6] = ".jpg";
	if (!cover)
		snprintf(ending, sizeof(ending) - 1, "_%x.jpg", position);

	std::string fname = file_name;
	if (p_movie_info)
		fname = p_movie_info->file.Name;

	/* quick check we have file and not url as file name */
	if (fname.c_str()[0] != '/') {
		if (autoshot)
			autoshot_done = true;
		return;
	}

	std::string::size_type pos = fname.find_last_of('.');
	if (pos != std::string::npos) {
		fname.replace(pos, fname.length(), ending);
	} else
		fname += ending;

	if (autoshot && !access(fname.c_str(), F_OK)) {
		printf("CMoviePlayerGui::makeScreenShot: cover [%s] already exist..\n", fname.c_str());
		autoshot_done = true;
		return;
	}

	if (!cover) {
		pos = fname.find_last_of('/');
		if (pos != std::string::npos)
			fname.replace(0, pos, g_settings.screenshot_dir);
	}


	CScreenShot * sc = new CScreenShot(fname);
	if (cover) {
		sc->EnableOSD(false);
		sc->EnableVideo(true);
	}
	if (autoshot || forcover) {
		int xres = 0, yres = 0, framerate;
		videoDecoder->getPictureInfo(xres, yres, framerate);
		if (xres && yres) {
			int w = std::min(300, xres);
			int h = (float) yres / ((float) xres / (float) w);
			sc->SetSize(w, h);
		}
	}
	sc->Start();
#endif
	if (autoshot)
		autoshot_done = true;
}

size_t CMoviePlayerGui::GetReadCount()
{
	uint64_t this_read = 0;
	this_read = playback->GetReadCount();
	uint64_t res;
	if (this_read < last_read)
		res = 0;
	else
		res = this_read - last_read;
	last_read = this_read;
	return (size_t) res;
}
