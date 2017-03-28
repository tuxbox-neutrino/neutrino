/*
	Framebuffer acceleration hardware abstraction functions.
	The hardware dependent acceleration functions for coolstream hd2 graphic chips
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

#include "fb_accel_cs_hdx_inc.h"

#define LOGTAG "[fb_accel_cs_hd2] "

CFbAccelCSHD2::CFbAccelCSHD2()
{
	fb_name = "Coolstream HD2 framebuffer";
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

void CFbAccelCSHD2::fbCopyArea(uint32_t width, uint32_t height, uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y)
{
	uint32_t  w_, h_;
	w_ = (width > xRes) ? xRes : width;
	h_ = (height > yRes) ? yRes : height;

	if(!(w_%4)) {
		fb_copyarea area;
		area.dx     = dst_x;
		area.dy     = dst_y;
		area.width  = w_;
		area.height = h_;
		area.sx     = src_x;
		area.sy     = src_y;
		ioctl(fd, FBIO_COPY_AREA, &area);
		//printf("\033[33m>>>>\033[0m [CFbAccelCSHD2::%s:%d] fb_copyarea w: %d, h: %d, dst_x: %d, dst_y: %d, src_x: %d, src_y: %d\n", __func__, __LINE__, w_, h_, dst_x, dst_y, src_x, src_y);
		return;
	}
	//printf("\033[31m>>>>\033[0m [CFbAccelCSHD2::%s:%d] sw blit w: %d, h: %d, dst_x: %d, dst_y: %d, src_x: %d, src_y: %d\n", __func__, __LINE__, w_, h_, dst_x, dst_y, src_x, src_y);
	CFrameBuffer::fbCopyArea(width, height, dst_x, dst_y, src_x, src_y);
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

void CFbAccelCSHD2::setOsdResolutions()
{
	/* FIXME: Infos available in driver? */
	osd_resolution_t res;
	osd_resolutions.clear();
	res.xRes = 1280;
	res.yRes = 720;
	res.bpp  = 32;
	res.mode = OSDMODE_720;
	osd_resolutions.push_back(res);
	if (fullHdAvailable()) {
		res.xRes = 1920;
		res.yRes = 1080;
		res.bpp  = 32;
		res.mode = OSDMODE_1080;
		osd_resolutions.push_back(res);
	}
}

int CFbAccelCSHD2::setMode(unsigned int nxRes, unsigned int nyRes, unsigned int nbpp)
{
	if (!available&&!active)
		return -1;

	if (osd_resolutions.empty())
		setOsdResolutions();

	unsigned int nxRes_ = nxRes;
	unsigned int nyRes_ = nyRes;
	unsigned int nbpp_  = nbpp;
	if (!fullHdAvailable()) {
		nxRes_ = 1280;
		nyRes_ = 720;
		nbpp_  = 32;
	}
	screeninfo.xres=nxRes_;
	screeninfo.yres=nyRes_;
	screeninfo.xres_virtual=nxRes_;
	screeninfo.yres_virtual=nyRes_*2;
	screeninfo.height=0;
	screeninfo.width=0;
	screeninfo.xoffset=screeninfo.yoffset=0;
	screeninfo.bits_per_pixel=nbpp_;

	if (ioctl(fd, FBIOPUT_VSCREENINFO, &screeninfo)<0)
		perror(LOGTAG "FBIOPUT_VSCREENINFO");

	printf(LOGTAG "SetMode: %dbits, red %d:%d green %d:%d blue %d:%d transp %d:%d\n",
	screeninfo.bits_per_pixel, screeninfo.red.length, screeninfo.red.offset, screeninfo.green.length, screeninfo.green.offset, screeninfo.blue.length, screeninfo.blue.offset, screeninfo.transp.length, screeninfo.transp.offset);
	if ((screeninfo.xres != nxRes_) ||
	    (screeninfo.yres != nyRes_) ||
	    (screeninfo.bits_per_pixel != nbpp_)) {
		printf(LOGTAG "SetMode failed: wanted: %dx%dx%d, got %dx%dx%d\n",
			   nxRes_, nyRes_, nbpp_,
			   screeninfo.xres, screeninfo.yres, screeninfo.bits_per_pixel);
		return -1;
	}

	fb_fix_screeninfo _fix;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &_fix) < 0) {
		perror(LOGTAG "FBIOGET_FSCREENINFO");
		return -1;
	}
	stride = _fix.line_length;
	swidth = stride / sizeof(fb_pixel_t);
	if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0)
		printf(LOGTAG "screen unblanking failed\n");
	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;
	printf(LOGTAG "%dx%dx%d line length %d. using %s graphics accelerator.\n", xRes, yRes, bpp, stride, _fix.id);

/*
max res 1280x720
	available	14745600
	stride		5120
max res 1920x1080
	available	16588800
	stride		7680
*/

	if (videoDecoder != NULL)
		videoDecoder->updateOsdScreenInfo();

	int needmem = stride * yRes * 2;
	if (available >= needmem)
	{
		backbuffer = lfb + swidth * yRes;
		return 0;
	}
	fprintf(stderr, LOGTAG "not enough FB memory (have %d, need %d)\n", available, needmem);
	backbuffer = lfb; /* will not work well, but avoid crashes */
	return 0; /* dont fail because of this */
}

fb_pixel_t * CFbAccelCSHD2::getBackBufferPointer() const
{
	return backbuffer;
}

void CFbAccelCSHD2::setBlendMode(uint8_t mode)
{
	uint8_t arg = CNXTFB_BLEND_MODE_PER_PIXEL;
	if (mode == 2)
		arg = CNXTFB_BLEND_MODE_UNIFORM_ALPHA;
	if (ioctl(fd, FBIO_SETBLENDMODE, arg))
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

int CFbAccelCSHD2::scale2Res(int size)
{
	/*
	   The historic resolution 1280x720 is default for some values/sizes.
	   So let's scale these values to other resolutions.
	*/

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	if (screeninfo.xres == 1920)
		size += size/2;
#endif

	return size;
}

bool CFbAccelCSHD2::fullHdAvailable()
{
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	if (available >= 16588800) /* new fb driver with maxres 1920x1080(*8) */
		return true;
#endif
	return false;
}

/* align for hw blit */
uint32_t CFbAccelCSHD2::getWidth4FB_HW_ACC(const uint32_t _x, const uint32_t _w, const bool max)
{
	uint32_t ret = _w;
	if ((_x + ret) >= xRes)
		ret = xRes-_x-1;
	if (ret%4 == 0)
		return ret;

	int add = (max) ? 3 : 0;
	ret = ((ret + add) / 4) * 4;
	if ((_x + ret) >= xRes)
		ret -= 4;

	return ret;
}
