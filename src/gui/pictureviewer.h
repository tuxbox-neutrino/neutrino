/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

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

#ifndef __pictureviewergui__
#define __pictureviewergui__


#include <driver/framebuffer.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <gui/widget/menue.h>
#include <gui/filebrowser.h>
#include <gui/audioplayer.h>

#include <string>

class CPicture
{
public:
	std::string Filename;
	std::string Name;
	std::string Type;
	time_t Date;
};

typedef std::vector<CPicture> CViewList;


class CPictureViewerGui : public CMenuTarget
{
	public:
		enum State
		{
			VIEW=0,
			MENU,
			SLIDESHOW
		};
		enum SortOrder
		{
			DATE=0,
			FILENAME,
			NAME
		};
	private:
		CFrameBuffer		*frameBuffer;
		CPictureViewer		*m_viewer;
		unsigned int		selected;
		unsigned int		liststart;
		unsigned int		listmaxshow;
		int					fheight; // Fonthoehe Playlist-Inhalt
		int					theight; // Fonthoehe Playlist-Titel
		int					sheight; // Fonthoehe MP Info
		int			footerHeight;
		int			buttons1Height;
		int			buttons2Height;
		bool				visible;			
		State          m_state;
		SortOrder      m_sort;

		CViewList			playlist;
		std::string			Path;

		int 			width;
		int 			height;
		int 			x;
		int 			y;
		int         m_title_w;
		long        m_time;

		int         m_LastMode;

		void paintItem(int pos);
		void paint();
		void paintHead();
		void paintFoot();
		void paintInfo();
		void paintLCD();
		void hide();

		CFileFilter picture_filter;
		void view(unsigned int nr, bool unscaled=false);
		void endView();
		int  show();

		void showHelp();
		void deletePicFile(unsigned int index, bool mode);

		bool audioplayer;
		int m_currentTitle;

		pthread_t	decodeT;
		static void*	decodeThread(void *arg);
		bool		decodeTflag;

		void thrView();
		bool m_unscaled;

	public:
		CPictureViewerGui();
		~CPictureViewerGui();
		int  exec(CMenuTarget* parent, const std::string & actionKey);

		CAudioPlayerGui *m_audioPlayer;
};


#endif


