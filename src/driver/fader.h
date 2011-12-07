/*
	Neutrino-HD GUI, COSDFader implementation
	Copyright (C) 2011 Stefan Seyfried

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __FADER_H__
#define __FADER_H__

#include <driver/framebuffer.h>

class COSDFader
{
	private:
		CFrameBuffer *fb;
		int step;
		int target_trans;
		int transparency;
		uint32_t timer;
	public:
		COSDFader(int trans);
		~COSDFader();

		void StartFadeIn();
		bool StartFadeOut();
		void Stop();
		bool Fade();
		uint32_t GetTimer(){ return timer; };
};
#endif
