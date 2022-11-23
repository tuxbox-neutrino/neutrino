/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2022 TangoCash

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

#ifndef __slide_f__
#define __slide_f__

#include <inttypes.h>

class COSDSlider
{
	private:
		uint32_t slideTimer;
		int slideValue;
		bool slideIn;
		bool slideOut;
		int proc_put(const char *path, char *value, int len);
		void setValue(int val);
		int slideMax;
		int slideMode;

	public:
		COSDSlider(int percent = 100, int mode = BOTTOM2TOP);
		virtual ~COSDSlider();

		void StartSlideIn();
		bool StartSlideOut();
		void StopSlide();
		bool SlideDone();
		bool SlideInDone() { return !slideIn && !slideOut; };
		uint32_t GetSlideTimer() { return slideTimer; };

		enum
		{
			BEFORE_SLIDEIN  = 1,
			AFTER_SLIDEIN   = 2,
			BEFORE_SLIDEOUT = 3,
			AFTER_SLIDEOUT  = 4
		};

		enum
		{
			MID2SIDE,
			BOTTOM2TOP
		};
};
#endif
