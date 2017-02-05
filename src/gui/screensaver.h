/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2012-2013 defans@bluepeercrew.us

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
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CSCREENSAVER_H__
#define __CSCREENSAVER_H__

#include <vector>
#include <string>
#include <gui/components/cc.h>

class CFrameBuffer;
class CPictureViewer;
class CScreenSaver : public sigc::trackable
{
	private:
		CFrameBuffer 	*m_frameBuffer;
		CPictureViewer	*m_viewer;
		CComponentsFrmClock *scr_clock;
		pthread_t	thrScreenSaver;
		static void*	ScreenSaverPrg(void *arg);
		vector<string> 	v_bg_files;
		unsigned int 	index;
		t_channel_id	pip_channel_id;
		bool		status_mute;

		bool ReadDir();
		void paint();

		union u_color {
			struct s_color {
				uint8_t b, g, r, a;
			} uc_color;
		unsigned int i_color;
		};

		u_color clr;

	public:
		enum
		{
			SCR_MODE_IMAGE,
			SCR_MODE_CLOCK,
			SCR_MODE_CLOCK_COLOR
		};
		CScreenSaver();
		~CScreenSaver();
		static CScreenSaver* getInstance();
		bool IsRun();
		void Start();
		void Stop();
		sigc::signal<void> OnBeforeStart;
		sigc::signal<void> OnAfterStop;
};

#endif // __CSCREENSAVER_H__
