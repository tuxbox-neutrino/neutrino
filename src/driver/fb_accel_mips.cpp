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
#include <driver/fb_accel_mips.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>

#include <stdlib.h>

#if ENABLE_MIPS_ACC
#include <driver/abstime.h>
#endif

#include <system/set_threadname.h>
#include <gui/color.h>

#define LOGTAG "[fb_accel_mips] "

#if ENABLE_MIPS_ACC
#define FBIO_BLIT   0x22
#endif
#define FBIO_ACCEL  0x23

static unsigned int displaylist[1024];
static int ptr;
static bool supportblendingflags = true;

#define P(x, y) do { displaylist[ptr++] = x; displaylist[ptr++] = y; } while (0)
#define C(x) P(x, 0)

static int fb_fd = -1;
static int exec_list(void);

#if ENABLE_MIPS_ACC
static bool accumulateoperations = false;

bool bcm_accel_has_alphablending()
{
	return supportblendingflags;
}

int bcm_accel_accumulate()
{
#ifdef SUPPORT_ACCUMULATED_ACCELERATION_OPERATIONS
	accumulateoperations = true;
	return 0;
#else
	return -1;
#endif
}

int bcm_accel_sync()
{
	int retval = 0;
	if (accumulateoperations)
	{
		if (ptr)
		{
			dprintf(DEBUG_NORMAL,"bcm_accel_sync: ptr %d\n", ptr);
			retval = exec_list();
		}
		accumulateoperations = false;
	}
	return retval;
}

void bcm_accel_blit(
		int src_addr, int src_width, int src_height, int src_stride, int src_format,
		int dst_addr, int dst_width, int dst_height, int dst_stride,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y, int dwidth, int dheight,
		int pal_addr, int flags)
{
	if (accumulateoperations)
	{
		if (((sizeof(displaylist) / sizeof(displaylist[0]) - ptr) / 2) < 40)
		{
			dprintf(DEBUG_NORMAL,"bcm_accel_blit: not enough space to accumulate\n");
			bcm_accel_sync();
			bcm_accel_accumulate();
		}
	}

	C(0x43); // reset source
	C(0x53); // reset dest
	C(0x5b);  // reset pattern
	C(0x67); // reset blend
	C(0x75); // reset output

	P(0x0, src_addr); // set source addr
	P(0x1, src_stride);  // set source pitch
	P(0x2, src_width); // source width
	P(0x3, src_height); // height
	switch (src_format)
	{
	case 0:
		P(0x4, 0x7e48888); // format: ARGB 8888
		break;
	case 1:
		P(0x4, 0x12e40008); // indexed 8bit
		P(0x78, 256);
		P(0x79, pal_addr);
		P(0x7a, 0x7e48888);
		break;
	}

	C(0x5); // set source surface (based on last parameters)

	P(0x2e, src_x); // define  rect
	P(0x2f, src_y);
	P(0x30, width);
	P(0x31, height);

	C(0x32); // set this rect as source rect

	P(0x0, dst_addr); // prepare output surface
	P(0x1, dst_stride);
	P(0x2, dst_width);
	P(0x3, dst_height);
	P(0x4, 0x7e48888);

	C(0x69); // set output surface

	P(0x2e, dst_x); // prepare output rect
	P(0x2f, dst_y);
	P(0x30, dwidth);
	P(0x31, dheight);

	C(0x6e); // set this rect as output rect

	if (supportblendingflags && flags) P(0x80, flags); /* blend flags... We'd really like some blending support in the drivers, to avoid punching holes in the osd */

	C(0x77);  // do it

	if (!accumulateoperations) exec_list();
}

void bcm_accel_fill(
		int dst_addr, int dst_width, int dst_height, int dst_stride,
		int x, int y, int width, int height,
		unsigned long color)
{
	if (accumulateoperations)
	{
		if (((sizeof(displaylist) / sizeof(displaylist[0]) - ptr) / 2) < 40)
		{
			dprintf(DEBUG_NORMAL,"bcm_accel_fill: not enough space to accumulate\n");
			bcm_accel_sync();
			bcm_accel_accumulate();
		}
	}

	C(0x43); // reset source
	C(0x53); // reset dest
	C(0x5b); // reset pattern
	C(0x67); // reset blend
	C(0x75); // reset output

	// clear dest surface
	P(0x0, 0);
	P(0x1, 0);
	P(0x2, 0);
	P(0x3, 0);
	P(0x4, 0);
	C(0x45);

	// clear src surface
	P(0x0, 0);
	P(0x1, 0);
	P(0x2, 0);
	P(0x3, 0);
	P(0x4, 0);
	C(0x5);

	P(0x2d, color);

	P(0x2e, x); // prepare output rect
	P(0x2f, y);
	P(0x30, width);
	P(0x31, height);
	C(0x6e); // set this rect as output rect

	P(0x0, dst_addr); // prepare output surface
	P(0x1, dst_stride);
	P(0x2, dst_width);
	P(0x3, dst_height);
	P(0x4, 0x7e48888);
	C(0x69); // set output surface

	P(0x6f, 0);
	P(0x70, 0);
	P(0x71, 2);
	P(0x72, 2);
	C(0x73); // select color keying

	C(0x77);  // do it

	if (!accumulateoperations) exec_list();
}
#endif

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

CFbAccelMIPS::CFbAccelMIPS()
{
#if ENABLE_MIPS_ACC
	blit_thread = false;
#endif
	fb_name  = "mipsbox framebuffer";
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
#if ENABLE_MIPS_ACC
	OpenThreads::Thread::start();
#endif
}

CFbAccelMIPS::~CFbAccelMIPS()
{
#if ENABLE_MIPS_ACC
	if (blit_thread)
	{
		blit_thread = false;
		blit(); /* wakes up the thread */
		OpenThreads::Thread::join();
	}
#endif
	if (fb_fd >= 0)
	{
		close(fb_fd);
		fb_fd = -1;
	}
}

void CFbAccelMIPS::setOsdResolutions()
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

int CFbAccelMIPS::setMode(unsigned int nxRes, unsigned int nyRes, unsigned int nbpp)
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

fb_pixel_t * CFbAccelMIPS::getBackBufferPointer() const
{
	return backbuffer;
}

int CFbAccelMIPS::scale2Res(int size)
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

bool CFbAccelMIPS::fullHdAvailable()
{
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	return true;
#endif
	return false;
}

#if ENABLE_MIPS_ACC
#define BLIT_INTERVAL_MIN 40
#define BLIT_INTERVAL_MAX 250
void CFbAccelMIPS::run()
{
	printf(LOGTAG "run start\n");
	int64_t last_blit = 0;
	blit_pending = false;
	blit_thread = true;
	blit_mutex.lock();
	set_threadname("mipsfb::autoblit");
	while (blit_thread) {
		blit_cond.wait(&blit_mutex, blit_pending ? BLIT_INTERVAL_MIN : BLIT_INTERVAL_MAX);
		int64_t now = time_monotonic_ms();
		if (now - last_blit < BLIT_INTERVAL_MIN)
		{
			blit_pending = true;
			//printf(LOGTAG "run: skipped, time %" PRId64 "\n", now - last_blit);
		}
		else
		{
			blit_pending = false;
			blit_mutex.unlock();
			_blit();
			blit_mutex.lock();
			last_blit = now;
		}
	}
	blit_mutex.unlock();
	printf(LOGTAG "run end\n");
}

void CFbAccelMIPS::blit()
{
	//printf(LOGTAG "blit\n");
	blit_mutex.lock();
	blit_cond.signal();
	blit_mutex.unlock();
}

void CFbAccelMIPS::_blit()
{
	if (ioctl(fd, FBIO_BLIT) < 0)
		printf("FBIO_BLIT failed\n");
}

void CFbAccelMIPS::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	if(dx <1 || dy <1 )
		return;

	// do not accelerate small areas
        if (fix.smem_start != 0 && dx > 25 && dy > 25)
		bcm_accel_fill(fix.smem_start, screeninfo.xres, screeninfo.yres, stride,x, y, dx, dy,col);

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
	//blit();
}
#endif
