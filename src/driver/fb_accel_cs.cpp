/*
	Framebuffer acceleration hardware abstraction functions.
	The hardware dependent acceleration functions for coolstream GXA chips
	are represented in this class.

	(C) 2017 Stefan Seyfried
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

#include <cs_api.h>
#include <cnxtfb.h>

/*******************************************************************************/
#define GXA_POINT(x, y)         (((y) & 0x0FFF) << 16) | ((x) & 0x0FFF)
#define GXA_SRC_BMP_SEL(x)      (x << 8)
#define GXA_DST_BMP_SEL(x)      (x << 5)
#define GXA_PARAM_COUNT(x)      (x << 2)

#define GXA_CMD_REG		0x001C
#define GXA_FG_COLOR_REG	0x0020
#define GXA_BG_COLOR_REG        0x0024
#define GXA_LINE_CONTROL_REG    0x0038
#define GXA_BMP1_TYPE_REG       0x0048
#define GXA_BMP1_ADDR_REG       0x004C
#define GXA_BMP2_TYPE_REG       0x0050
#define GXA_BMP2_ADDR_REG       0x0054
#define GXA_BMP3_TYPE_REG       0x0058
#define GXA_BMP3_ADDR_REG       0x005C
#define GXA_BMP4_TYPE_REG       0x0060
#define GXA_BMP4_ADDR_REG       0x0064
#define GXA_BMP5_TYPE_REG       0x0068
#define GXA_BMP5_ADDR_REG       0x006C
#define GXA_BMP6_TYPE_REG       0x0070
#define GXA_BMP7_TYPE_REG       0x0078
#define GXA_DEPTH_REG		0x00F4
#define GXA_CONTENT_ID_REG      0x0144

#define GXA_CMD_BLT             0x00010800
#define GXA_CMD_NOT_ALPHA       0x00011000
#define GXA_CMD_NOT_TEXT        0x00018000
#define GXA_CMD_QMARK		0x00001000

#define GXA_BLEND_CFG_REG       0x003C
#define GXA_CFG_REG             0x0030
#define GXA_CFG2_REG            0x00FC

#define LOGTAG "[fb_accel_gxa] "
/*
static unsigned int _read_gxa(volatile unsigned char *base_addr, unsigned int offset)
{
	return *(volatile unsigned int *)(base_addr + offset);
}
*/
static unsigned int _mark = 0;

static void _write_gxa(volatile unsigned char *base_addr, unsigned int offset, unsigned int value)
{
	while ((*(volatile unsigned int *)(base_addr + GXA_DEPTH_REG)) & 0x40000000) {};
	*(volatile unsigned int *)(base_addr + offset) = value;
}

/* this adds a tagged marker into the GXA queue. Once this comes out
   of the other end of the queue, all commands before it are finished */
void CFbAccelCS::add_gxa_sync_marker(void)
{
	unsigned int cmd = GXA_CMD_QMARK | GXA_PARAM_COUNT(1);
	// TODO: locking?
	_mark++;
	_mark &= 0x0000001F; /* bit 0x20 crashes the kernel, if set */
	_write_gxa(gxa_base, cmd, _mark);
	//fprintf(stderr, "%s: wrote %02x\n", __FUNCTION__, _mark);
}

/* wait until the current marker comes out of the GXA command queue */
void CFbAccelCS::waitForIdle(const char *func)
{
	unsigned int cfg, count = 0;
	do {
		cfg = *(volatile unsigned int *)(gxa_base + GXA_CMD_REG);
		cfg >>= 24;	/* the token is stored in bits 31...24 */
		if (cfg == _mark)
			break;
		/* usleep is too coarse, because of CONFIG_HZ=100 in kernel
		   so use sched_yield to at least give other threads a chance to run */
		sched_yield();
		//fprintf(stderr, "%s: read  %02x, expected %02x\n", __FUNCTION__, cfg, _mark);
	} while(++count < 8192); /* don't deadlock here if there is an error */

	if (count > 2048) /* more than 2000 are unlikely, even for large BMP6 blits */
		fprintf(stderr, LOGTAG "waitForIdle: count is big (%d) [%s]!\n", count, func?func:"");
}

CFbAccelCS::CFbAccelCS()
{
	fb_name = "Coolstream NEVIS GXA framebuffer";
}

void CFbAccelCS::init(const char * const)
{
fprintf(stderr, ">FBACCEL::INIT\n");
	CFrameBuffer::init();
	if (lfb == NULL) {
		printf(LOGTAG "CFrameBuffer::init() failed.\n");
		return; /* too bad... */
	}
	available = fix.smem_len;
	printf(LOGTAG "%dk video mem\n", available / 1024);
	memset(lfb, 0, available);
	lastcol = 0xffffffff;
	lbb = lfb;	/* the memory area to draw to... */

	/* Open /dev/mem for HW-register access */
	devmem_fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (devmem_fd < 0) {
		perror("CFbAccel open /dev/mem");
		goto error;
	}

	/* mmap the GXA's base address */
	gxa_base = (volatile unsigned char*)mmap(0, 0x00040000, PROT_READ|PROT_WRITE, MAP_SHARED, devmem_fd, 0xE0600000);
	if (gxa_base == (void *)-1) {
		perror("CFbAccel mmap /dev/mem");
		goto error;
	}

	setupGXA();
 error:
	/* TODO: what to do here? does this really happen? */
	;
};

CFbAccelCS::~CFbAccelCS()
{
	if (gxa_base != MAP_FAILED)
		munmap((void *)gxa_base, 0x40000);
	if (devmem_fd != -1)
		close(devmem_fd);
	if (lfb)
		munmap(lfb, available);
	if (fd > -1)
		close(fd);
}

void CFbAccelCS::setColor(fb_pixel_t col)
{
	if (col == lastcol)
		return;
	_write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int)col); /* setup the drawing color */
	lastcol = col;
}

void CFbAccelCS::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	unsigned int cmd = GXA_CMD_BLT | GXA_CMD_NOT_TEXT | GXA_CMD_NOT_ALPHA |
			   GXA_SRC_BMP_SEL(6) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2);

	_write_gxa(gxa_base, GXA_BG_COLOR_REG, (unsigned int)col); /* setup the drawing color */
	_write_gxa(gxa_base, GXA_BMP6_TYPE_REG, (3 << 16) | (1 << 27)); /* 3 == 32bpp, 1<<27 == fill */
	_write_gxa(gxa_base, cmd, GXA_POINT(x, y));	/* destination pos */
	_write_gxa(gxa_base, cmd, GXA_POINT(dx, dy));	/* destination size */
	_write_gxa(gxa_base, GXA_BG_COLOR_REG, (unsigned int)backgroundColor);

	/* the GXA seems to do asynchronous rendering, so we add a sync marker
	   to which the fontrenderer code can synchronize */
	add_gxa_sync_marker();
}

void CFbAccelCS::paintPixel(const int x, const int y, const fb_pixel_t col)
{
	paintLine(x, y, x + 1, y, col);
}

void CFbAccelCS::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	/* draw a single vertical line from point xa/ya to xb/yb */
	unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

	setColor(col);
	_write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404);	/* X is major axis, skip last pixel */
	_write_gxa(gxa_base, cmd, GXA_POINT(xb, yb));		/* end point */
	_write_gxa(gxa_base, cmd, GXA_POINT(xa, ya));		/* start point */
}

void CFbAccelCS::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	int  xc, yc;
	xc = (width > xRes) ? xRes : width;
	yc = (height > yRes) ? yRes : height;
	(void)transp;
	u32 cmd;
	void *uKva;

	uKva = cs_phys_addr(fbbuff);
	//printf("CFbAccelCS::blit2FB: data %x Kva %x\n", (int) fbbuff, (int) uKva);

	if (uKva != NULL) {
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		cmd = GXA_CMD_BLT | GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(1) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(3);

		_write_gxa(gxa_base, GXA_BMP1_TYPE_REG, (3 << 16) | width);
		_write_gxa(gxa_base, GXA_BMP1_ADDR_REG, (unsigned int)uKva);

		_write_gxa(gxa_base, cmd, GXA_POINT(xoff, yoff)); /* destination pos */
		_write_gxa(gxa_base, cmd, GXA_POINT(xc, yc));     /* source width, FIXME real or adjusted xc, yc ? */
		_write_gxa(gxa_base, cmd, GXA_POINT(xp, yp));     /* source pos */

		return;
	}
#if 0
	for(int i = 0; i < yc; i++){
		memmove(clfb + (i + yoff)*stride + xoff*4, ip + (i + yp)*width + xp, xc*4);
	}
#endif
}

void CFbAccelCS::setupGXA()
{
	// We (re)store the GXA regs here in case DFB override them and was not
	// able to restore them.
	_write_gxa(gxa_base, GXA_BMP2_TYPE_REG, (3 << 16) | (unsigned int)screeninfo.xres);
	_write_gxa(gxa_base, GXA_BMP2_ADDR_REG, (unsigned int) fix.smem_start);
	_write_gxa(gxa_base, GXA_BLEND_CFG_REG, 0x00089064);
	//	TODO check mono-flip, bit 8
	_write_gxa(gxa_base, GXA_CFG_REG, 0x100 | (1 << 12) | (1 << 29));
	_write_gxa(gxa_base, GXA_CFG2_REG, 0x1FF);
	_write_gxa(gxa_base, GXA_BG_COLOR_REG, (unsigned int)backgroundColor);
	add_gxa_sync_marker();
}

/* wrong name... */
int CFbAccelCS::setMode(unsigned int, unsigned int, unsigned int)
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

fb_pixel_t * CFbAccelCS::getBackBufferPointer() const
{
	return backbuffer;
}

void CFbAccelCS::setBlendMode(uint8_t mode)
{
	if (ioctl(fd, FBIO_SETBLENDMODE, mode))
		printf("FBIO_SETBLENDMODE failed.\n");
}

void CFbAccelCS::setBlendLevel(int level)
{
	unsigned char value = 0xFF;
	if (level >= 0 && level <= 100)
		value = convertSetupAlpha2Alpha(level);

	if (ioctl(fd, FBIO_SETOPACITY, value))
		printf("FBIO_SETOPACITY failed.\n");
#ifndef BOXMODEL_APOLLO
	if (level == 100) // TODO: sucks.
		usleep(20000);
#endif
}
