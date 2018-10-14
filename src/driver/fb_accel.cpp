/*
	Framebuffer acceleration hardware abstraction functions.
	The infrastructure for hardware dependent framebuffer acceleration
	functions are implemented in this class, hardware specific stuff
	is in derived classes.

	(C) 2017 Stefan Seyfried

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/fb_generic.h>
#include <driver/fb_accel.h>

CFbAccel::CFbAccel()
{
};

CFbAccel::~CFbAccel()
{
};

void CFbAccel::paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius, int type)
{
	/* draw a filled rectangle (with additional round corners) */
	if (!getActive())
		return;

	checkFbArea(x, y, dx, dy, true);

	if (!type || !radius)
	{
		paintRect(x, y, dx, dy, col);
		checkFbArea(x, y, dx, dy, false);
		return;
	}

	setCornerFlags(type);
	/* limit the radius */
	radius = limitRadius(dx, dy, radius);
	if (radius < 1)		/* dx or dy = 0... */
		radius = 1;	/* avoid div by zero below */

	int line = 0;
	while (line < dy) {
		int ofl, ofr;
		if (calcCorners(NULL, &ofl, &ofr, dy, line, radius, type)) {
			int height = dy - ((corner_tl || corner_tr)?radius: 0 ) - ((corner_bl || corner_br) ? radius : 0);
			paintRect(x, y + line, dx, height, col);
			line += height;
			continue;
		}
		if (dx - ofr - ofl < 1) {
			//printf("FB-NG::%s:%d x %d y %d dx %d dy %d l %d r %d\n", __func__, __LINE__, x,y,dx,dy, ofl, ofr);
			line++;
			continue;
		}
		paintHLineRelInternal(x + ofl, dx - ofl - ofr, y + line, col);
		line++;
	}
	checkFbArea(x, y, dx, dy, false);
	mark(x, y, x+dx, y+dy);
}

void CFbAccel::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	int line = 0;
	fb_pixel_t *fbp = getFrameBufferPointer() + (swidth * y);
	int pos;
	while (line < dy)
	{
		for (pos = x; pos < x + dx; pos++)
			*(fbp + pos) = col;
		fbp += swidth;
		line++;
	}
	mark(x, y, x+dx, y+dy);
	blit();
}
