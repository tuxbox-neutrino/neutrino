/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
                      2003 thegoodguy
	Copyright (C) 2007-2013 Stefan Seyfried

	Framebuffer acceleration hardware abstraction functions.
	The various hardware dependent framebuffer acceleration functions
	are represented in this class.

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

#include <driver/framebuffer.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>
#include <math.h>

#include <linux/kd.h>

#include <stdlib.h>

#if HAVE_COOL_HARDWARE
#include <cs_api.h>
#include <cnxtfb.h>
#endif

//#undef USE_NEVIS_GXA //FIXME
/*******************************************************************************/
#ifdef USE_NEVIS_GXA
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
void CFbAccel::add_gxa_sync_marker(void)
{
	unsigned int cmd = GXA_CMD_QMARK | GXA_PARAM_COUNT(1);
	// TODO: locking?
	_mark++;
	_mark &= 0x0000001F; /* bit 0x20 crashes the kernel, if set */
	_write_gxa(gxa_base, cmd, _mark);
	//fprintf(stderr, "%s: wrote %02x\n", __FUNCTION__, _mark);
}

/* wait until the current marker comes out of the GXA command queue */
void CFbAccel::waitForIdle(void)
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
	} while(++count < 2048); /* don't deadlock here if there is an error */

	if (count > 512) /* more than 100 are unlikely, */
		fprintf(stderr, "CFbAccel::waitForIdle: count is big (%d)!\n", count);
}
#elif HAVE_TRIPLEDRAGON
#include <directfb.h>
#include <tdgfx/stb04gfx.h>
extern IDirectFB *dfb;
extern IDirectFBSurface *dfbdest;
extern int gfxfd;
void CFbAccel::waitForIdle(void)
{
#if 0
	struct timeval ts, te;
	gettimeofday(&ts, NULL);
#endif
	/* does not work: DFBResult r = dfb->WaitForSync(dfb); */
	ioctl(gfxfd, STB04GFX_ENGINE_SYNC);
#if 0
	gettimeofday(&te, NULL);
	printf("STB04GFX_ENGINE_SYNC took %lld us\n", (te.tv_sec * 1000000LL + te.tv_usec) - (ts.tv_sec * 1000000LL + ts.tv_usec));
#endif
}
#else
void CFbAccel::waitForIdle(void)
{
}
#endif

CFbAccel::CFbAccel(CFrameBuffer *_fb)
{
	fb = _fb;
	lastcol = 0xffffffff;
	lbb = fb->lfb;	/* the memory area to draw to... */

#ifdef USE_NEVIS_GXA
	/* Open /dev/mem for HW-register access */
	devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
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
#endif /* USE_NEVIS_GXA */
};

CFbAccel::~CFbAccel()
{
#ifdef USE_NEVIS_GXA
	if (gxa_base != MAP_FAILED)
		munmap((void *)gxa_base, 0x40000);
	if (devmem_fd != -1)
		close(devmem_fd);
#endif
}

void CFbAccel::update()
{
}

void CFbAccel::setColor(fb_pixel_t col)
{
#if HAVE_TRIPLEDRAGON
	if (col == lastcol)
		return;
	char *c = (char *)&col;
	dfbdest->SetColor(dfbdest, c[1], c[2], c[3], c[0]);
	lastcol = col;
#elif defined USE_NEVIS_GXA
	if (col == lastcol)
		return;
	_write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int)col); /* setup the drawing color */
	lastcol = col;
#else
	(void)col; /* avoid "unused parameter" compiler warning */
#endif
}

void CFbAccel::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
#if HAVE_TRIPLEDRAGON
	setColor(col);
	dfbdest->FillRectangle(dfbdest, x, y, dx, dy);
#elif defined(USE_NEVIS_GXA)
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	unsigned int cmd = GXA_CMD_BLT | GXA_CMD_NOT_TEXT | GXA_CMD_NOT_ALPHA |
			   GXA_SRC_BMP_SEL(6) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2);

	_write_gxa(gxa_base, GXA_BG_COLOR_REG, (unsigned int)col); /* setup the drawing color */
	_write_gxa(gxa_base, GXA_BMP6_TYPE_REG, (3 << 16) | (1 << 27)); /* 3 == 32bpp, 1<<27 == fill */
	_write_gxa(gxa_base, cmd, GXA_POINT(x, y));	/* destination pos */
	_write_gxa(gxa_base, cmd, GXA_POINT(dx, dy));	/* destination size */
	_write_gxa(gxa_base, GXA_BG_COLOR_REG, (unsigned int)fb->backgroundColor);

	/* the GXA seems to do asynchronous rendering, so we add a sync marker
	   to which the fontrenderer code can synchronize */
	add_gxa_sync_marker();
#else
	int line = 0;
	int swidth = fb->stride / sizeof(fb_pixel_t);
	fb_pixel_t *fbp = fb->getFrameBufferPointer() + (swidth * y);
	int pos;
	while (line < dy)
	{
		for (pos = x; pos < x + dx; pos++)
			*(fbp + pos) = col;
		fbp += swidth;
		line++;
	}
#endif
}

void CFbAccel::paintPixel(const int x, const int y, const fb_pixel_t col)
{
#if HAVE_TRIPLEDRAGON || defined (USE_NEVIS_GXA)
	paintLine(x, y, x, y, col);
#else
	fb_pixel_t *pos = fb->getFrameBufferPointer();
	pos += (fb->stride / sizeof(fb_pixel_t)) * y;
	pos += x;
	*pos = col;
#endif
}

void CFbAccel::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
#if HAVE_TRIPLEDRAGON
	setColor(col);
	dfbdest->DrawLine(dfbdest, xa, ya, xb, yb);
#elif defined(USE_NEVIS_GXA)
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	/* draw a single vertical line from point xa/ya to xb/yb */
	unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

	setColor(col);
	_write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404);	/* X is major axis, skip last pixel */
	_write_gxa(gxa_base, cmd, GXA_POINT(xb, yb));		/* end point */
	_write_gxa(gxa_base, cmd, GXA_POINT(xa, ya));		/* start point */
#else
	int dx = abs (xa - xb);
	int dy = abs (ya - yb);
	int x;
	int y;
	int End;
	int step;

	if (dx > dy)
	{
		int p = 2 * dy - dx;
		int twoDy = 2 * dy;
		int twoDyDx = 2 * (dy-dx);

		if (xa > xb)
		{
			x = xb;
			y = yb;
			End = xa;
			step = ya < yb ? -1 : 1;
		}
		else
		{
			x = xa;
			y = ya;
			End = xb;
			step = yb < ya ? -1 : 1;
		}

		paintPixel(x, y, col);

		while (x < End)
		{
			x++;
			if (p < 0)
				p += twoDy;
			else
			{
				y += step;
				p += twoDyDx;
			}
			paintPixel(x, y, col);
		}
	}
	else
	{
		int p = 2 * dx - dy;
		int twoDx = 2 * dx;
		int twoDxDy = 2 * (dx-dy);

		if (ya > yb)
		{
			x = xb;
			y = yb;
			End = ya;
			step = xa < xb ? -1 : 1;
		}
		else
		{
			x = xa;
			y = ya;
			End = yb;
			step = xb < xa ? -1 : 1;
		}

		paintPixel(x, y, col);

		while (y < End)
		{
			y++;
			if (p < 0)
				p += twoDx;
			else
			{
				x += step;
				p += twoDxDy;
			}
			paintPixel(x, y, col);
		}
	}
#endif
}

#if !HAVE_TRIPLEDRAGON
void CFbAccel::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	int  xc, yc;

	xc = (width > fb->xRes) ? fb->xRes : width;
	yc = (height > fb->yRes) ? fb->yRes : height;

#ifdef USE_NEVIS_GXA
	u32 cmd;
	void *uKva;

	uKva = cs_phys_addr(fbbuff);
	//printf("CFbAccel::blit2FB: data %x Kva %x\n", (int) fbbuff, (int) uKva);

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
#else
	fb_pixel_t *data = (fb_pixel_t *) fbbuff;

	uint8_t *d = ((uint8_t *)fb->getFrameBufferPointer()) + xoff * sizeof(fb_pixel_t) + fb->stride * yoff;
	fb_pixel_t * d2;

	for (int count = 0; count < yc; count++ ) {
		fb_pixel_t *pixpos = &data[(count + yp) * width];
		d2 = (fb_pixel_t *) d;
		for (int count2 = 0; count2 < xc; count2++ ) {
			fb_pixel_t pix = *(pixpos + xp);
			if (!transp || (pix & 0xff000000) == 0xff000000)
				*d2 = pix;
			else {
				uint8_t *in = (uint8_t *)(pixpos + xp);
				uint8_t *out = (uint8_t *)d2;
				int a = in[3];	/* TODO: big/little endian */
				*out = (*out + ((*in - *out) * a) / 256);
				in++; out++;
				*out = (*out + ((*in - *out) * a) / 256);
				in++; out++;
				*out = (*out + ((*in - *out) * a) / 256);
			}
			d2++;
			pixpos++;
		}
		d += fb->stride;
	}
#if 0
	for(int i = 0; i < yc; i++){
		memmove(clfb + (i + yoff)*stride + xoff*4, ip + (i + yp)*width + xp, xc*4);
	}
#endif
#endif
}
#else
void CFbAccel::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	DFBRectangle src;
	DFBResult err;
	IDirectFBSurface *surf;
	DFBSurfaceDescription dsc;

	src.x = xp;
	src.y = yp;
	src.w = width - xp;
	src.h = height - yp;

	dsc.flags  = (DFBSurfaceDescriptionFlags)(DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PREALLOCATED);
	dsc.caps   = DSCAPS_NONE;
	dsc.width  = width;
	dsc.height = height;
	dsc.preallocated[0].data  = fbbuff;
	dsc.preallocated[0].pitch = width * sizeof(fb_pixel_t);
	err = dfb->CreateSurface(dfb, &dsc, &surf);
	/* TODO: maybe we should not die if this fails? */
	if (err != DFB_OK) {
		fprintf(stderr, "CFbAccel::blit2FB: ");
		DirectFBErrorFatal("dfb->CreateSurface(dfb, &dsc, &surf)", err);
	}

	if (transp)
	{
		surf->SetSrcColorKey(surf, 0, 0, 0);
		dfbdest->SetBlittingFlags(dfbdest, DSBLIT_SRC_COLORKEY);
	}
	else
		dfbdest->SetBlittingFlags(dfbdest, DSBLIT_NOFX);
	dfbdest->Blit(dfbdest, surf, &src, xoff, yoff);
	surf->Release(surf);
	return;
}
#endif

#ifdef USE_NEVIS_GXA
void CFbAccel::setupGXA()
{
	// We (re)store the GXA regs here in case DFB override them and was not
	// able to restore them.
	_write_gxa(gxa_base, GXA_BMP2_TYPE_REG, (3 << 16) | (unsigned int)fb->screeninfo.xres);
	_write_gxa(gxa_base, GXA_BMP2_ADDR_REG, (unsigned int) fb->fix.smem_start);
	_write_gxa(gxa_base, GXA_BLEND_CFG_REG, 0x00089064);
	//	TODO check mono-flip, bit 8
	_write_gxa(gxa_base, GXA_CFG_REG, 0x100 | (1 << 12) | (1 << 29));
	_write_gxa(gxa_base, GXA_CFG2_REG, 0x1FF);
	_write_gxa(gxa_base, GXA_BG_COLOR_REG, (unsigned int)fb->backgroundColor);
}
#endif

#if HAVE_AZBOX_HARDWARE

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif
#ifndef FBIO_BLIT
#define FBIO_BLIT 0x22
#define FBIO_SET_MANUAL_BLIT _IOW('F', 0x21, __u8)
#endif
void CFbAccel::blit()
{
	// blit
	if (ioctl(fb->fd, FBIO_BLIT) < 0)
		perror("CFbAccel FBIO_BLIT");
#if 0
	// sync bliter
	int c = 0;
	if( ioctl(fd, FBIO_WAITFORVSYNC, &c) < 0 )
		perror("FBIO_WAITFORVSYNC");
#endif
}

#else
/* not azbox and not spark -> no blit() needed */
void CFbAccel::blit()
{
}
#endif

/* not really used yet */
#ifdef PARTIAL_BLIT
void CFbAccel::mark(int xs, int ys, int xe, int ye)
{
	update_dirty(xs, ys, xe, ye);
}
#else
void CFbAccel::mark(int, int, int, int)
{
}
#endif

