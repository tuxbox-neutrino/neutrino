/*
 * abstract fb_window class - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __fb_window_h__
#define __fb_window_h__
#include <inttypes.h>

class CFrameBuffer;
class CFBWindow
{
 public:
	typedef unsigned int color_t;
	typedef void *         font_t;

 private:
	CFrameBuffer	* frameBuffer;
	fb_pixel_t	* Background;

 public:
	int x, y;   /* upper left corner */
	int dx, dy; /* dimension         */

	CFBWindow(const int _x, const int _y, const int _dx, const int _dy);
	~CFBWindow();

	void paintBoxRel(const int _x, const int _y, const int _dx, const int _dy, const color_t _col, int radius = 0, int type = 0xF);
	void paintBoxFrame(int _x, int _y, int _dx, int _dy, int _px, const color_t _col, int radius = 0, int type = 0xf);
	void paintVLineRel(int _x, int _y, int _dy, const color_t _col);
	void paintHLineRel(int _x, int _dx, int _y, const color_t _col);
	bool paintIcon(const char * const _filename, const int _x, const int _y, const int _h = 0, const color_t _offset = 1);
	void RenderString(const font_t _font, const int _x, const int _y, const int _width, const char * const _text, const color_t _color, const int _boxheight = 0, const unsigned int _flags = 1 /*Font::IS_UTF8*/);

	fb_pixel_t* saveScreen(const int _x, const int _y, const int _dx, const int _dy);
	void restoreScreen(const int _x, const int _y, const int _dx, const int _dy, fb_pixel_t* buf, bool delBuf);
};

#endif /* __fb_window_h__ */
