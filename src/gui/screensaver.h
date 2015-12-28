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

#include <driver/framebuffer.h>
#include <vector>
#include <string>
#include <gui/components/cc.h>

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

		bool		status_mute;

		bool ReadDir();
		void paint();

	public:
		enum
		{
			SCR_MODE_IMAGE,
			SCR_MODE_CLOCK
		};
		CScreenSaver();
		~CScreenSaver();
		static CScreenSaver* getInstance();

		void Start();
		void Stop();
		sigc::signal<void> OnBeforeStart;
		sigc::signal<void> OnAfterStop;
};

#endif // __CSCREENSAVER_H__
