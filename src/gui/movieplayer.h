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
	const char     *filename;
	bool		stopped;
	CMoviePlayerGui::state playstate;
	bool isBookmark;
	bool isMovieBrowser;
	int speed;
	off64_t fullposition;
	int startposition;
	int jumpseconds;
	bool showaudioselectdialog;
	off64_t minuteoffset;
	off64_t secondoffset;
	std::string startfilename;

	std::string Path_local;
	static  unsigned short g_numpida;
	static unsigned short g_vtype;
	static unsigned short g_vpid;
	static std::string    g_language[REC_MAX_APIDS];
	const static short MOVIE_HINT_BOX_TIMER = 5;	// time to show bookmark hints in seconds

	CFileBrowser * filebrowser;
	CMovieBrowser* moviebrowser;

	CBookmarkManager * bookmarkmanager;

	CFileFilter tsfilefilter;

	static cPlayback *playback;
	static CMoviePlayerGui* instance_mp;

	void Init(void);
	void PlayFile();
	void cutNeutrino();
	void restoreNeutrino();

	void showHelpTS(void);
	void showHelpVLC(void);
	void callInfoViewer(MI_MOVIE_INFO * p_movie_info, const int duration, const int pos);
	void fillPids(MI_MOVIE_INFO * p_movie_info);

	CMoviePlayerGui(const CMoviePlayerGui&) {};

 public:
	CMoviePlayerGui();
	~CMoviePlayerGui();

	static CMoviePlayerGui& getInstance();
	static void Delete();
	
	int exec(CMenuTarget* parent, const std::string & actionKey);
	void updateLcd(const std::string & sel_filename);
	bool Playing();
	int timeshift;
	bool start_timeshift;
	int file_prozent;
};


class CAPIDSelectExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
		static unsigned short g_apids[REC_MAX_APIDS];
		static unsigned short g_ac3flags[REC_MAX_APIDS];
		static unsigned short g_currentapid, g_currentac3, apidchanged;
};

#endif
