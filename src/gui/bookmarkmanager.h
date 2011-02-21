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

#ifndef __bookmarkmanager__
#define __bookmarkmanager__

#include <config.h>
#include <configfile.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <driver/framebuffer.h>
#include <gui/widget/menue.h>

#define MAXBOOKMARKS 10
#define BOOKMARKFILE "/var/tuxbox/config/bookmarks"

class CBookmark
{
 private:
	std::string name;
	std::string url;
	std::string time;
 public:
	CBookmark(const std::string & name, const std::string & url, const std::string & time);
	const char *getName(void) const { return name.c_str(); };
	const char *getUrl (void) const { return url .c_str(); };
	const char *getTime(void) const { return time.c_str(); };
	inline void setName(const std::string & new_name) { name = new_name; };
	inline void setUrl (const std::string & new_url ) { url  = new_url ; };
	inline void setTime(const std::string & new_time) { time = new_time; };
};

//-----------------------------------------

class CBookmarkManager
{
 private:
	std::vector<CBookmark> bookmarks;
	CConfigFile	bookmarkfile;
	
    CFrameBuffer		*frameBuffer;
	unsigned int		selected;
	unsigned int		liststart;
	unsigned int		listmaxshow;
	int			fheight; // Fonthoehe Timerlist-Inhalt
	int			theight; // Fonthoehe Timerlist-Titel
	int			footerHeight;
	bool			visible;			
	int 			width;
	int 			height;
	int 			x;
	int 			y;

	
	//int bookmarkCount;
	bool bookmarksmodified;
	void readBookmarkFile();
	void writeBookmarkFile();
	CBookmark getBookmark();
	int addBookmark(CBookmark inBookmark);
	
	void paintItem(int pos);
	void paint();
	void paintHead();
	void paintFoot();
	void hide();



 public:
	CBookmarkManager();
	~CBookmarkManager();
	int createBookmark(const std::string & name, const std::string & url, const std::string & time);
	int createBookmark(const std::string & url, const std::string & time);
	void removeBookmark(unsigned int index);
	void renameBookmark(unsigned int index);
	int getBookmarkCount(void) const;
	int getMaxBookmarkCount(void) const;
	void flush();
	
	const CBookmark * getBookmark(CMenuTarget* parent);
};

#endif
