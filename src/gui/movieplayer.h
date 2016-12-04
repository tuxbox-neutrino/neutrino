/*
  Neutrino-GUI  -   DBoxII-Project

  Copyright (C) 2003,2004 gagga
  Homepage: http://www.giggo.de/dbox

  Kommentar:

  Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
  Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
  auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
  Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#ifndef __movieplayergui__
#define __movieplayergui__

#include <config.h>
#include <configfile.h>
#include <driver/framebuffer.h>
#include <gui/filebrowser.h>
#include <gui/bookmarkmanager.h>
#include <gui/widget/menue.h>
#include <gui/moviebrowser/mb.h>
#include <gui/movieinfo.h>
#include <gui/widget/hintbox.h>
#include <gui/timeosd.h>
#include <driver/record.h>
#include <playback.h>

#include <stdio.h>

#include <string>
#include <vector>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class CMoviePlayerGui : public CMenuTarget
{
 public:
	enum state
		{
		    STOPPED     =  0,
		    PREPARING   =  1,
		    STREAMERROR =  2,
		    PLAY        =  3,
		    PAUSE       =  4,
		    FF          =  5,
		    REW         =  6
		};

	enum
		{
		    PLUGIN_PLAYSTATE_NORMAL    = 0,
		    PLUGIN_PLAYSTATE_STOP      = 1,
		    PLUGIN_PLAYSTATE_NEXT      = 2,
		    PLUGIN_PLAYSTATE_PREV      = 3,
		    PLUGIN_PLAYSTATE_LEAVE_ALL = 4
		};

	enum repeat_mode_enum { REPEAT_OFF = 0, REPEAT_TRACK = 1, REPEAT_ALL = 2 };

 private:
	typedef struct livestream_info_t
	{
		std::string url;
		std::string name;
		std::string resolution;
		std::string header;//cookie
		int res1;
		int bandwidth;
	} livestream_info_struct_t;

	std::string livestreamInfo1;
	std::string livestreamInfo2;

	CFrameBuffer * frameBuffer;
	int            m_LastMode;	

	std::string	file_name;
	std::string	pretty_name;
	std::string	cookie_header;
	std::string	info_1, info_2;
	std::string    	currentaudioname;
	bool		playing;
	bool		time_forced;
	CMoviePlayerGui::state playstate;
	int keyPressed;
	bool isLuaPlay;
	bool haveLuaInfoFunc;
	lua_State* luaState;
	bool blockedFromPlugin;
	int speed;
	int startposition;
	int position;
	int duration;
	CTimeOSD FileTime;

	unsigned short numpida;
	unsigned short vpid;
	unsigned short vtype;
	std::string    language[MAX_PLAYBACK_PIDS];
	unsigned short apids[MAX_PLAYBACK_PIDS];
	unsigned short ac3flags[MAX_PLAYBACK_PIDS];
	unsigned short currentapid, currentac3;
	repeat_mode_enum repeat_mode;

	/* screensaver */
	int		m_idletime;
	bool		m_screensaver;
	void		screensaver(bool on);

	/* subtitles vars */
	unsigned short numsubs;
	std::string    slanguage[MAX_PLAYBACK_PIDS];
	unsigned short spids[MAX_PLAYBACK_PIDS];
	unsigned short sub_supported[MAX_PLAYBACK_PIDS];
	int currentspid;
	int min_x, min_y, max_x, max_y;
	time_t end_time;
	bool ext_subs;
	bool lock_subs;
	uint64_t last_read;

	/* playback from MB */
	bool isMovieBrowser;
	bool isHTTP;
	bool isUPNP;
	bool isWebTV;
	bool isYT;
	bool showStartingHint;
	static CMovieBrowser* moviebrowser;
	MI_MOVIE_INFO * p_movie_info;
	MI_MOVIE_INFO movie_info;
	P_MI_MOVIE_LIST milist;
	const static short MOVIE_HINT_BOX_TIMER = 5;	// time to show bookmark hints in seconds

	/* playback from file */
	bool is_file_player;
	bool iso_file;
	bool stopped;
	CFileBrowser * filebrowser;
	CFileFilter tsfilefilter;
	CFileList filelist;
	CFileList::iterator filelist_it;
	CFileList::iterator vzap_it;
	void set_vzap_it(bool up);
	bool fromInfoviewer;
	std::string Path_local;
	int menu_ret;
	bool autoshot_done;
	//std::vector<livestream_info_t> liveStreamList;

	/* playback from bookmark */
	static CBookmarkManager * bookmarkmanager;
	bool isBookmark;

	static OpenThreads::Mutex mutex;
	static OpenThreads::Mutex bgmutex;
	static OpenThreads::Condition cond;
	static pthread_t bgThread;
	static bool webtv_started;

	static cPlayback *playback;
	static CMoviePlayerGui* instance_mp;
	static CMoviePlayerGui* instance_bg;

	void Init(void);
	void PlayFile();
	bool PlayFileStart();
	void PlayFileLoop();
	void PlayFileEnd(bool restore = true);
	void cutNeutrino();
	bool StartWebtv();

	void quickZap(neutrino_msg_t msg);
	void showHelp(void);
	void callInfoViewer(bool init_vzap_it = true);
	void fillPids();
	bool getAudioName(int pid, std::string &apidtitle);
	void getCurrentAudioName( bool file_player, std::string &audioname);
	void addAudioFormat(int count, std::string &apidtitle, bool& enabled );

	void handleMovieBrowser(neutrino_msg_t msg, int position = 0);
	bool SelectFile();
	void updateLcd();

	bool convertSubtitle(std::string &text);
	void selectChapter();
	void selectAutoLang();
	void parsePlaylist(CFile *file);
	bool mountIso(CFile *file);
	void makeFilename();
	bool prepareFile(CFile *file);
	void makeScreenShot(bool autoshot = false, bool forcover = false);

	void Cleanup();
	void ClearFlags();
	void ClearQueue();
	void EnableClockAndMute(bool enable);
	static void *ShowStartHint(void *arg);
	static void* bgPlayThread(void *arg);
	static bool sortStreamList(livestream_info_t info1, livestream_info_t info2);
	bool selectLivestream(std::vector<livestream_info_t> &streamList, int res, livestream_info_t* info);
	bool luaGetUrl(const std::string &script, const std::string &file, std::vector<livestream_info_t> &streamList);

	CMoviePlayerGui(const CMoviePlayerGui&) {};
	CMoviePlayerGui();

 public:
	~CMoviePlayerGui();

	static CMoviePlayerGui& getInstance(bool background = false);

	int exec(CMenuTarget* parent, const std::string & actionKey);
	bool Playing() { return playing; };
	bool osdTimeVisible() { return FileTime.IsVisible(); };
	std::string CurrentAudioName() { return currentaudioname; };
	int GetSpeed() { return speed; }
	int GetPosition() { return position; }
	int GetDuration() { return duration; }
	void UpdatePosition();
	int timeshift;
	int file_prozent;
	void SetFile(std::string &name, std::string &file, std::string info1="", std::string info2="") { pretty_name = name; file_name = file; info_1 = info1; info_2 = info2; }
	bool PlayBackgroundStart(const std::string &file, const std::string &name, t_channel_id chan, const std::string &script="");
	void stopPlayBack(void);
	void setLastMode(int m) { m_LastMode = m; }
	void Pause(bool b = true);
	void selectAudioPid();
	bool SetPosition(int pos, bool absolute = false);
	void selectSubtitle();
	void showSubtitle(neutrino_msg_data_t data);
	void clearSubtitle(bool lock = false);
	int getKeyPressed() { return keyPressed; };
	size_t GetReadCount();
	std::string GetFile() { return pretty_name; }
	void restoreNeutrino();
	void setFromInfoviewer(bool f) { fromInfoviewer = f; };
	void setBlockedFromPlugin(bool b) { blockedFromPlugin = b; };
	bool getBlockedFromPlugin() { return blockedFromPlugin; };
	void setLuaInfoFunc(lua_State* L, bool func) { luaState = L; haveLuaInfoFunc = func; };
	void getLivestreamInfo(std::string *i1, std::string *i2) { *i1=livestreamInfo1; *i2=livestreamInfo2; };
	bool getLiveUrl(const t_channel_id chan, const std::string &url, const std::string &script, std::string &realUrl, std::string &_pretty_name, std::string &info1, std::string &info2, std::string &header);
};

#endif
