/*
	Neutrino-GUI  -   DBoxII-Project

	TimeOSD by Zwen
	
	Homepage: http://dbox.cyberphoria.org/

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

#ifndef __timeosd__
#define __timeosd__

#include <time.h>
#include <driver/framebuffer.h>
#include <gui/components/cc.h>

class CTimeOSD
{
	public:
		enum mode
		{
			MODE_ASC,
			MODE_DESC
		};
	
	private:
		CFrameBuffer		*frameBuffer;
		CProgressBar		*timescale;

		time_t m_time_show;
		bool visible;
		int m_xstart,m_xend,m_y,m_height, m_width, t1;
		mode m_mode;
		void GetDimensions();

	public:
		CTimeOSD();
		~CTimeOSD();
		void show(time_t time_show);
		void update(time_t time_show);
		void updatePos(short runningPercent);
		void hide();
		bool IsVisible() {return visible;}
		void SetMode(mode m) { m_mode = m;}
		mode GetMode() { return m_mode;}
		void update(int position, int duration);
		void switchMode(int position, int duration);
};
#endif
