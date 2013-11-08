/*
	Mediaplayer selection menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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

#ifndef __MEDIAPLAYER__
#define __MEDIAPLAYER__

#include <gui/widget/menue.h>
#include <gui/audioplayer.h>
#include <gui/personalize.h>

#include <string>

class CMediaPlayerMenu : public CMenuTarget
{
	private:
		int width, usage_mode;
		neutrino_locale_t menu_title;
		
		CAudioPlayerGui *audioPlayer;
		CAudioPlayerGui *inetPlayer;
				
		void showMoviePlayer(CMenuWidget *menu_movieplayer, CPersonalizeGui *p);

	public:	
		enum MM_MENU_MODES
		{
			MODE_DEFAULT,
			MODE_AUDIO,
			MODE_VIDEO
		};
		
		CMediaPlayerMenu();
		~CMediaPlayerMenu();
		static CMediaPlayerMenu* getInstance();
		
		int initMenuMedia(CMenuWidget *m = NULL, CPersonalizeGui *p = NULL);
		
		int exec(CMenuTarget* parent, const std::string & actionKey);
		void setMenuTitel(const neutrino_locale_t title = LOCALE_MAINMENU_MEDIA){menu_title = title;};
		void setUsageMode(const int& mm_mode = MODE_DEFAULT){usage_mode = mm_mode;};
		CAudioPlayerGui *getPlayerInstance() { if (audioPlayer != NULL) return audioPlayer; else if (inetPlayer != NULL) return inetPlayer; else return NULL; }
};


#endif
