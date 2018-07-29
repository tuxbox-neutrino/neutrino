/*
	Framebuffer acceleration hardware abstraction functions.
	The hardware dependent acceleration functions for bcm graphic chips
	are represented in this class.

	(C) 2017 TangoCash
	Derived from old neutrino-hd framebuffer code / e2 sources

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
#include <driver/fb_accel_arm.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>

#include <stdlib.h>

#include <system/set_threadname.h>
#include <gui/color.h>

#define LOGTAG "[fb_accel_arm] "

#define FBIO_ACCEL  0x23

static unsigned int displaylist[1024];
static int ptr;
static bool supportblendingflags = true;

#define P(x, y) do { displaylist[ptr++] = x; displaylist[ptr++] = y; } while (0)
#define C(x) P(x, 0)

static int fb_fd = -1;
static int exec_list(void);

static int exec_list(void)
{
	int ret;
	struct
	{
		void *ptr;
		int len;
	} l;

	if (fb_fd < 0) return -1;

	l.ptr = displaylist;
	l.len = ptr;
	ret = ioctl(fb_fd, FBIO_ACCEL, &l);
	ptr = 0;
	return ret;
}

CFbAccelARM::CFbAccelARM()
{
	fb_name  = "armbox framebuffer";
	fb_fd = open(FB_DEVICE, O_RDWR);
	if (fb_fd < 0)
	{
		printf(LOGTAG "[bcm] %s %m", FB_DEVICE);
	}
	if (exec_list())
	{
		printf(LOGTAG "[bcm] interface not available - %m");
		close(fb_fd);
		fb_fd = -1;
	}
	/* now test for blending flags support */
	P(0x80, 0);
	if (exec_list())
	{
		supportblendingflags = false;
	}
#ifdef FORCE_NO_BLENDING_ACCELERATION
	/* hardware doesn't allow us to detect whether the opcode is working */
	supportblendingflags = false;
#endif
}

CFbAccelARM::~CFbAccelARM()
{
	if (fb_fd >= 0)
	{
		close(fb_fd);
		fb_fd = -1;
	}
}

void CFbAccelARM::setOsdResolutions()
{
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

int CFbAccelARM::setMode(unsigned int nxRes, unsigned int nyRes, unsigned int nbpp)
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

fb_pixel_t * CFbAccelARM::getBackBufferPointer() const
{
	return backbuffer;
}

int CFbAccelARM::scale2Res(int size)
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

bool CFbAccelARM::fullHdAvailable()
{
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	return true;
#endif
	return false;
}
