/*
	color gradient - Neutrino-GUI
	Copyright (C) 2014 M. Liebmann (micha-bbg)

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CCOLORGRADIENT__
#define __CCOLORGRADIENT__

#include <inttypes.h>

class CFrameBuffer;
class CColorGradient
{
	private:
		CFrameBuffer * frameBuffer;

		inline uint8_t limitChar(int c);

	public:
		///gradient mode
		enum {
			gradientDark2Light,
			gradientLight2Dark,
			gradientDark2Light2Dark,
			gradientLight2Dark2Light
		};

		///intensity
		enum {
			light,
			normal,
			extended
		};

		CColorGradient();
		~CColorGradient();
//		static CColorGradient* getInstance();

		fb_pixel_t* gradientOneColor(fb_pixel_t col, fb_pixel_t *gradientBuf, int bSize, int mode, int intensity=normal, uint8_t v_min=0x40, uint8_t v_max=0xE0, uint8_t s=0xC0);
		fb_pixel_t* gradientColorToTransparent(fb_pixel_t col, fb_pixel_t *gradientBuf, int bSize, int mode, int intensity=normal);
		fb_pixel_t* gradientColorToColor(fb_pixel_t start_col, fb_pixel_t end_col, fb_pixel_t *gradientBuf, int bSize, int mode, int intensity=normal);

};

#endif // __CCOLORGRADIENT__
