/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/src/driver/fb_window.cpp,v 1.6 2004/03/14 22:28:13 thegoodguy Exp $
 *
 * abstract fb_window class - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2009-2012 Stefan Seyfried
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/fb_window.h>

#include <driver/fontrenderer.h>
#include <driver/framebuffer.h>

CFBWindow::CFBWindow(const int _x, const int _y, const int _dx, const int _dy)
{
	x  = _x ;
	y  = _y ;
	dx = _dx;
	dy = _dy;

	frameBuffer = CFrameBuffer::getInstance();
	Background = saveScreen(_x, _y, _dx, _dy);
}

CFBWindow::~CFBWindow(void)
{
	if (Background != NULL)
		restoreScreen(x, y, dx, dy, Background, true);
}

fb_pixel_t* CFBWindow::saveScreen(const int _x, const int _y, const int _dx, const int _dy)
{
	fb_pixel_t* buf = new fb_pixel_t [_dx * _dy];
	if (buf != NULL)
		frameBuffer->SaveScreen(_x, _y, _dx, _dy, buf);
	return buf;
}

void CFBWindow::restoreScreen(const int _x, const int _y, const int _dx, const int _dy, fb_pixel_t* buf, bool delBuf)
{
	if (buf != NULL)
		frameBuffer->RestoreScreen(_x, _y, _dx, _dy, buf);
	if (delBuf)
		delete[] buf;
}

void CFBWindow::paintBoxRel(const int _x, const int _y, const int _dx, const int _dy, const color_t _col, int radius, int type)
{
	frameBuffer->paintBoxRel(x + _x, y + _y, _dx, _dy, _col, radius, type);
}

void CFBWindow::paintBoxFrame(int _x, int _y, int _dx, int _dy, int _px, const color_t _col, int radius, int type)
{
	frameBuffer->paintBoxFrame(x + _x, y + _y, _dx, _dy, _px, _col, radius, type);
}

void CFBWindow::paintVLineRel(int _x, int _y, int _dy, const color_t _col)
{
	frameBuffer->paintVLineRel(x + _x, y + _y, _dy, _col);
}

void CFBWindow::paintHLineRel(int _x, int _dx, int _y, const color_t _col)
{
	frameBuffer->paintHLineRel(x + _x, _dx, y + _y, _col);
}

bool CFBWindow::paintIcon(const char * const _filename, const int _x, const int _y, const int _h, const color_t _offset)
{
	frameBuffer->paintIcon(_filename, x + _x, y + _y, _h, _offset);
	return 0;
}

void CFBWindow::RenderString(const font_t _font, const int _x, const int _y, const int _width, const char * const _text, const color_t _color, const int _boxheight, const unsigned int _flags)
{
	((Font *)_font)->RenderString(x + _x, y + _y, _width, _text, _color, _boxheight, _flags);
}
