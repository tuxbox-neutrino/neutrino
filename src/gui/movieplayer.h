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
#include <gui/filebrowser.h>
#include <gui/bookmarkmanager.h>
#include <gui/widget/menue.h>
#include <gui/moviebrowser/mb.h>
#include <driver/movieinfo.h>
#include <gui/widget/hintbox.h>
#include <gui/timeosd.h>
#include <driver/record.h>
#include <zapit/channel.h>
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

class CFrameBuffer;
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
	int currentVideoSystem;
	uint32_t currentOsdResolution;

	unsigned int numpida;
	int vpid;
	int vtype;
	std::string    language[REC_MAX_APIDS];
#if HAVE_COOL_HARDWARE
	uint16_t apids[REC_MAX_APIDS];
	unsigned short ac3flags[REC_MAX_APIDS];
#else
	int apids[REC_MAX_APIDS];
	unsigned int ac3flags[REC_MAX_APIDS];
#endif
	int currentapid, currentac3;
	repeat_mode_enum repeat_mode;

	/* screensaver */
	int		m_idletime;
	bool		m_screensaver;
	void		screensaver(bool on);

	// subtitle data
	unsigned int numpids;
#ifndef REC_MAX_SPIDS
#define REC_MAX_SPIDS 20 // whatever
#endif
	std::string slanguage[REC_MAX_SPIDS];
	int spids[REC_MAX_SPIDS];

	// teletext subtitle data
	unsigned int numpidt;
#ifndef REC_MAX_TPIDS
#define REC_MAX_TPIDS 50 // not pids, actually -- a pid may cover multiple subtitle pages
#endif
	std::string tlanguage[REC_MAX_TPIDS];
	int tpids[REC_MAX_TPIDS];
	int tmag[REC_MAX_TPIDS];
	int tpage[REC_MAX_TPIDS];
	std::string currentttxsub;

#if 0
	/* subtitles vars */
	unsigned short numsubs;
	std::string    slanguage[REC_MAX_APIDS];
	unsigned short spids[REC_MAX_APIDS];
	unsigned short sub_supported[REC_MAX_APIDS];
	int currentspid;
	int min_x, min_y, max_x, max_y;
	int64_t end_time;
	bool ext_subs;
	bool lock_subs;
#endif
	uint64_t last_read;

	/* playback from MB */
	bool isMovieBrowser;
	bool isHTTP;
	bool isUPNP;
	bool isWebChannel;
	bool isYT;
	bool showStartingHint;
	static CMovieBrowser* moviebrowser;
	MI_MOVIE_INFO movie_info;
	P_MI_MOVIE_LIST milist;
	const static short MOVIE_HINT_BOX_TIMER = 5;	// time to show bookmark hints in seconds
#if HAVE_SPARK_HARDWARE
	CFrameBuffer::Mode3D old3dmode;
#endif

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

#if 0
	void selectSubtitle();
	bool convertSubtitle(std::string &text);
	void selectChapter();
#endif
	void selectAutoLang();
	void parsePlaylist(CFile *file);
	bool mountIso(CFile *file);
	void makeFilename();
	bool prepareFile(CFile *file);
	void makeScreenShot(bool autoshot = false, bool forcover = false);

	void Cleanup();
	void ClearFlags();
	void ClearQueue();
	void enableOsdElements(bool mute);
	void disableOsdElements(bool mute);
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

	MI_MOVIE_INFO * p_movie_info;
	int exec(CMenuTarget* parent, const std::string & actionKey);
	bool Playing() { return playing; };
	std::string CurrentAudioName() { return currentaudioname; };
	int GetSpeed() { return speed; }
	int GetPosition() { return position; }
	int GetDuration() { return duration; }
	int getState() { return playstate; }
	void UpdatePosition();
	int timeshift;
	int file_prozent;
	cPlayback *getPlayback() { return playback; }
	void SetFile(std::string &name, std::string &file, std::string info1="", std::string info2="") { pretty_name = name; file_name = file; info_1 = info1; info_2 = info2; }
	unsigned int getAPID(void);
	unsigned int getAPID(unsigned int i);
	void getAPID(int &apid, unsigned int &is_ac3);
	bool getAPID(unsigned int i, int &apid, unsigned int &is_ac3);
	bool setAPID(unsigned int i);
	unsigned int getAPIDCount(void);
	std::string getAPIDDesc(unsigned int i);
	unsigned int getSubtitleCount(void);
	CZapitAbsSub* getChannelSub(unsigned int i, CZapitAbsSub **s);
	int getCurrentSubPid(CZapitAbsSub::ZapitSubtitleType st);
	void setCurrentTTXSub(const char *s) { currentttxsub = s; }
	t_channel_id getChannelId(void);
	bool PlayBackgroundStart(const std::string &file, const std::string &name, t_channel_id chan, const std::string &script="");
	void stopPlayBack(void);
	void StopSubtitles(bool enable_glcd_mirroring);
	void StartSubtitles(bool show = true);
	void setLastMode(int m) { m_LastMode = m; }
	void Pause(bool b = true);
	void selectAudioPid(void);
	bool SetPosition(int pos, bool absolute = false);
#if 0
	void selectSubtitle();
	void showSubtitle(neutrino_msg_data_t data);
	void clearSubtitle(bool lock = false);
#endif
	int getKeyPressed() { return keyPressed; };
	size_t GetReadCount();
	std::string GetFile() { return pretty_name; }
	void restoreNeutrino();
	void setFromInfoviewer(bool f) { fromInfoviewer = f; };
	void setBlockedFromPlugin(bool b) { blockedFromPlugin = b; };
	bool getBlockedFromPlugin() { return blockedFromPlugin; };
	void setLuaInfoFunc(lua_State* L, bool func) { luaState = L; haveLuaInfoFunc = func; };
	void getLivestreamInfo(std::string *i1, std::string *i2) { *i1=livestreamInfo1; *i2=livestreamInfo2; };
	bool getLiveUrl(const std::string &url, const std::string &script, std::string &realUrl, std::string &_pretty_name, std::string &info1, std::string &info2, std::string &header);
};

#endif
