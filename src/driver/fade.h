/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __fade_f__
#define __fade_f__

#include <driver/framebuffer.h>

class COSDFader
{
	private:
		uint32_t	fadeTimer;
		int		fadeValue;
		unsigned char	& max_alpha;
		bool		fadeIn;
		bool		fadeOut;

		CFrameBuffer            *frameBuffer;
	public:
		COSDFader(unsigned char &alpha);
		virtual ~COSDFader();

		void StartFadeIn();
		bool StartFadeOut();
		void StopFade();
		bool FadeDone();
		uint32_t GetFadeTimer() { return fadeTimer; };
};
#endif
