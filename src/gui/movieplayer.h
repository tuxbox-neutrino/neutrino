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
#include "driver/framebuffer.h"
#include "gui/filebrowser.h"
#include "gui/bookmarkmanager.h"
#include "gui/widget/menue.h"
#include "gui/moviebrowser.h"
#include "gui/movieinfo.h"
#include <gui/widget/hintbox.h>
#include <driver/record.h>
#include <playback.h>

#include <stdio.h>

#include <string>
#include <vector>

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
		    REW         =  6,
		};

 private:
	CFrameBuffer * frameBuffer;
	int            m_LastMode;	

	std::string	full_name;
	std::string	file_name;

	bool		playing;
	CMoviePlayerGui::state playstate;
	int speed;
	int startposition;

	unsigned short numpida;
	unsigned short vpid;
	unsigned short vtype;
	std::string    language[REC_MAX_APIDS];
	unsigned short apids[REC_MAX_APIDS];
	unsigned short ac3flags[REC_MAX_APIDS];
	unsigned short currentapid, currentac3;

	/* playback from MB */
	bool isMovieBrowser;
	CMovieBrowser* moviebrowser;
	MI_MOVIE_INFO * p_movie_info;
	const static short MOVIE_HINT_BOX_TIMER = 5;	// time to show bookmark hints in seconds

	/* playback from file */
	bool is_file_player;
	CFileBrowser * filebrowser;
	CFileFilter tsfilefilter;
	std::string Path_local;

	/* playback from bookmark */
	CBookmarkManager * bookmarkmanager;
	bool isBookmark;

	cPlayback *playback;
	static CMoviePlayerGui* instance_mp;

	void Init(void);
	void PlayFile();
	void cutNeutrino();
	void restoreNeutrino();

	void showHelpTS(void);
	void callInfoViewer(const int duration, const int pos);
	void fillPids();
	bool getAudioName(int pid, std::string &apidtitle);
	void selectAudioPid(bool file_player);

	void handleMovieBrowser(neutrino_msg_t msg, int position = 0);
	bool SelectFile();
	void updateLcd();

	CMoviePlayerGui(const CMoviePlayerGui&) {};
	CMoviePlayerGui();

 public:
	~CMoviePlayerGui();

	static CMoviePlayerGui& getInstance();

	int exec(CMenuTarget* parent, const std::string & actionKey);
	bool Playing() { return playing; };
	int timeshift;
	int file_prozent;
};

#endif
