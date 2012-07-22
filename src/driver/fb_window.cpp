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
	Background = new fb_pixel_t [_dx * _dy];
	if (Background != NULL)
		frameBuffer->SaveScreen(_x, _y, _dx, _dy, Background);

}

CFBWindow::~CFBWindow(void)
{
	if (Background != NULL)
		frameBuffer->RestoreScreen(x, y, dx, dy, Background);
	delete[] Background;
}

void CFBWindow::paintBoxRel(const int _x, const int _y, const int _dx, const int _dy, const color_t _col, int radius, int type)
{
	frameBuffer->paintBoxRel(x + _x, y + _y, _dx, _dy, _col, radius, type);
	frameBuffer->blit();
}

bool CFBWindow::paintIcon(const char * const _filename, const int _x, const int _y, const int _h, const color_t _offset)
{
	frameBuffer->paintIcon(_filename, x + _x, y + _y, _h, _offset);
	frameBuffer->blit();
	return 0;
}

void CFBWindow::RenderString(const font_t _font, const int _x, const int _y, const int _width, const char * const _text, const color_t _color, const int _boxheight, const bool _utf8_encoded)
{
	((Font *)_font)->RenderString(x + _x, y + _y, _width, _text, _color, _boxheight, _utf8_encoded);
	frameBuffer->blit();
}
