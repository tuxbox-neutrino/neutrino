/*
	Framebuffer acceleration hardware abstraction functions.
	The common functions for coolstream hd1/hd2 graphic chips
	are represented in this class.

	(C) 2017 M. Liebmann

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

#include "fb_accel_cs_hdx_inc.h"

#define LOGTAG "[fb_accel_cs_hdx] "

CFbAccelCSHDx::CFbAccelCSHDx()
{
	fb_name   = "CST HDx framebuffer";
}

/*
CFbAccelCSHDx::~CFbAccelCSHDx()
{
}
*/

int CFbAccelCSHDx::fbCopy(uint32_t *mem_p, int width, int height,
			  int dst_x, int dst_y, int src_x, int src_y, int mode)
{
	if (videoDecoder == NULL) {
		if (dst_y < (int)yRes) {
			uint32_t src_y_ = src_y;
			if (mode == CS_FBCOPY_BB2FB)
				src_y_ += yRes;
			CFrameBuffer::fbCopyArea(width, height, dst_x, dst_y, src_x, src_y_);
			return 0;
		}
		return -1;
	}

	mutex.lock();
	setActive(false);
	int ret = videoDecoder->fbCopy(mem_p, width, height, dst_x, dst_y, src_x, src_y, mode);
	add_gxa_sync_marker();
	setActive(true);
	mutex.unlock();
	return ret;
}

int CFbAccelCSHDx::fbFill(int sx, int sy, int width, int height, fb_pixel_t color, int mode/*=0*/)
{
	if (videoDecoder == NULL) {
		CFbAccel::paintRect(sx, sy, width, height, color);
		return 0;
	}

	mutex.lock();
	setActive(false);
	int ret = videoDecoder->fbFill(sx, sy, width, height, color, mode);
	add_gxa_sync_marker();
	setActive(true);
	mutex.unlock();
	return ret;
}

#if 0
/* TODO: Run this functions with hardware acceleration */
void CFbAccelCSHDx::SaveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive())
		return;

	checkFbArea(x, y, dx, dy, true);
	fb_pixel_t * pos = getFrameBufferPointer() + x + swidth * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++) {
		fb_pixel_t * dest = (fb_pixel_t *)pos;
		for (int i = 0; i < dx; i++)
			*(bkpos++) = *(dest++);
		pos += swidth;
	}
	checkFbArea(x, y, dx, dy, false);
printf("%s\n", __func_ext__);
}

void CFbAccelCSHDx::RestoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive())
		return;

	checkFbArea(x, y, dx, dy, true);
	fb_pixel_t * fbpos = getFrameBufferPointer() + x + swidth * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memmove(fbpos, bkpos, dx * sizeof(fb_pixel_t));
		fbpos += swidth;
		bkpos += dx;
	}
	mark(x, y, x + dx, y + dy);
	checkFbArea(x, y, dx, dy, false);
printf("%s\n", __func_ext__);
}

void CFbAccelCSHDx::Clear()
{
	paintBackground();
printf("%s\n", __func_ext__);
}
#endif
