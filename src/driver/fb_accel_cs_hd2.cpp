/*
	Framebuffer acceleration hardware abstraction functions.
	The hardware dependent acceleration functions for coolstream apollo graphic chips
	are represented in this class.

	(C) 2017 M. Liebmann
	Derived from old neutrino-hd framebuffer code

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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>
#include <limits.h>
#include <gui/color.h>
#include <system/debug.h>

#include <cs_api.h>
#include <cnxtfb.h>

#define LOGTAG "[fb_accel_apollo] "

CFbAccelCSHD2::CFbAccelCSHD2()
{
	fb_name = "Coolstream APOLLO framebuffer";
}

/*
CFbAccelCSHD2::~CFbAccelCSHD2()
{
}
*/

void CFbAccelCSHD2::paintHLineRel(int x, int dx, int y, const fb_pixel_t col)
{
	if (!getActive())
		return;

	if (dx >= 10) {
		fb_fillrect fillrect;
		fillrect.dx = x;
		fillrect.dy = y;
		fillrect.width = dx;
		fillrect.height = 1;
		fillrect.color = col;
		fillrect.rop = ROP_COPY;
		ioctl(fd, FBIO_FILL_RECT, &fillrect);
		return;
	}
	CFrameBuffer::paintHLineRelInternal(x, dx, y, col);
}

void CFbAccelCSHD2::paintVLineRel(int x, int y, int dy, const fb_pixel_t col)
{
	if (!getActive())
		return;

	fb_fillrect fillrect;
	fillrect.dx = x;
	fillrect.dy = y;
	fillrect.width = 1;
	fillrect.height = dy;
	fillrect.color = col;
	fillrect.rop = ROP_COPY;
	ioctl(fd, FBIO_FILL_RECT, &fillrect);
}

void CFbAccelCSHD2::paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius, int type)
{
	if (!getActive())
		return;

	if (dx == 0 || dy == 0) {
		dprintf(DEBUG_DEBUG, "[CFbAccelCSHD2] [%s - %d]: radius %d, start x %d y %d end x %d y %d\n", __func__, __LINE__, radius, x, y, x+dx, y+dy);
		return;
	}
	if (radius < 0)
		dprintf(DEBUG_NORMAL, "[CFbAccelCSHD2] [%s - %d]: WARNING! radius < 0 [%d] FIXME\n", __func__, __LINE__, radius);

	checkFbArea(x, y, dx, dy, true);

	fb_fillrect fillrect;
	fillrect.color	= col;
	fillrect.rop	= ROP_COPY;

	if (type && radius) {
		setCornerFlags(type);
		radius = limitRadius(dx, dy, radius);

		int line = 0;
		while (line < dy) {
			int ofl, ofr;
			if (calcCorners(NULL, &ofl, &ofr, dy, line, radius, type)) {
				int rect_height_mult = ((type & CORNER_TOP) && (type & CORNER_BOTTOM)) ? 2 : 1;
				fillrect.dx	= x;
				fillrect.dy	= y + line;
				fillrect.width	= dx;
				fillrect.height	= dy - (radius * rect_height_mult);

				ioctl(fd, FBIO_FILL_RECT, &fillrect);
				line += dy - (radius * rect_height_mult);
				continue;
			}

			if (dx-ofr-ofl < 1) {
				if (dx-ofr-ofl == 0){
					dprintf(DEBUG_INFO, "[CFbAccelCSHD2] [%s - %d]: radius %d, start x %d y %d end x %d y %d\n", __func__, __LINE__, radius, x, y, x+dx-ofr-ofl, y+line);
				}else{
					dprintf(DEBUG_INFO, "[CFbAccelCSHD2] [%s - %04d]: Calculated width: %d\n                      (radius %d, dx %d, offsetLeft %d, offsetRight %d).\n                      Width can not be less than 0, abort.\n",
						__func__, __LINE__, dx-ofr-ofl, radius, dx, ofl, ofr);
				}
				line++;
				continue;
			}
			paintHLineRel(x+ofl, dx-ofl-ofr, y+line, col);
			line++;
		}
	} else {
		/* FIXME small size faster to do by software */
		if (dx > 10 || dy > 10) {
			fillrect.dx	= x;
			fillrect.dy	= y;
			fillrect.width	= dx;
			fillrect.height	= dy;
			ioctl(fd, FBIO_FILL_RECT, &fillrect);
			checkFbArea(x, y, dx, dy, false);
			return;
		}
		int line = 0;
		while (line < dy) {
			CFrameBuffer::paintHLineRelInternal(x, dx, y+line, col);
			line++;
		}
	}
	checkFbArea(x, y, dx, dy, false);
}

void CFbAccelCSHD2::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	int  xc, yc;
	xc = (width > xRes) ? xRes : width;
	yc = (height > yRes) ? yRes : height;

	if(!(width%4)) {
		fb_image image;
		image.dx = xoff;
		image.dy = yoff;
		image.width = xc;
		image.height = yc;
		image.cmap.len = 0;
		image.depth = 32;
		image.data = (const char*)fbbuff;
		ioctl(fd, FBIO_IMAGE_BLT, &image);
		return;
	}
	CFrameBuffer::blit2FB(fbbuff, width, height, xoff, yoff, xp, yp, transp);
}

void CFbAccelCSHD2::blitBox2FB(const fb_pixel_t* boxBuf, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff)
{
	if(width <1 || height <1 || !boxBuf )
		return;

	uint32_t xc = (width > xRes) ? (uint32_t)xRes : width;
	uint32_t yc = (height > yRes) ? (uint32_t)yRes : height;

	if (!(width%4)) {
		fb_image image;
		image.dx = xoff;
		image.dy = yoff;
		image.width = xc;
		image.height = yc;
		image.cmap.len = 0;
		image.depth = 32;
		image.data = (const char*)boxBuf;
		ioctl(fd, FBIO_IMAGE_BLT, &image);
		return;
	}
	CFrameBuffer::blitBox2FB(boxBuf, width, height, xoff, yoff);
}

int CFbAccelCSHD2::setMode(unsigned int, unsigned int, unsigned int)
{
	fb_fix_screeninfo _fix;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &_fix) < 0) {
		perror("FBIOGET_FSCREENINFO");
		return -1;
	}
	stride = _fix.line_length;
	if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0)
		printf("screen unblanking failed\n");
	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;
	printf(LOGTAG "%dx%dx%d line length %d. using apollo graphics accelerator.\n", xRes, yRes, bpp, stride);
	int needmem = stride * yRes * 2;
	if (available >= needmem)
	{
		backbuffer = lfb + stride / sizeof(fb_pixel_t) * yRes;
		return 0;
	}
	fprintf(stderr, LOGTAG "not enough FB memory (have %d, need %d)\n", available, needmem);
	backbuffer = lfb; /* will not work well, but avoid crashes */
	return 0; /* dont fail because of this */
}

void CFbAccelCSHD2::setBlendMode(uint8_t mode)
{
	if (ioctl(fd, FBIO_SETBLENDMODE, mode))
		printf("FBIO_SETBLENDMODE failed.\n");
}

void CFbAccelCSHD2::setBlendLevel(int level)
{
	unsigned char value = 0xFF;
	if (level >= 0 && level <= 100)
		value = convertSetupAlpha2Alpha(level);

	if (ioctl(fd, FBIO_SETOPACITY, value))
		printf("FBIO_SETOPACITY failed.\n");
	if (level == 100) // TODO: sucks.
		usleep(20000);
}
