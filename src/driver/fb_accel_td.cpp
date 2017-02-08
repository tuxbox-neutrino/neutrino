/*
	Framebuffer acceleration hardware abstraction functions.
	The hardware dependent framebuffer acceleration functions for the
	TripleDragon are represented in this class using DirectFB.

	License: GPL

	(C) 2017 Stefan Seyfried

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
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>

#include <stdlib.h>

#include <system/set_threadname.h>
#include <gui/color.h>

#define LOGTAG "[fb_accel_td] "

#include <directfb.h>
#include <tdgfx/stb04gfx.h>
extern IDirectFB *dfb;
extern IDirectFBSurface *dfbdest;
extern int gfxfd;
void CFbAccelTD::waitForIdle(const char *)
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

CFbAccelTD::CFbAccelTD()
{
	fb_name = "TripleDragon framebuffer";
	lastcol = 0xffffffff;
	lbb = lfb;	/* the memory area to draw to... */
};

CFbAccelTD::~CFbAccelTD()
{
	if (lfb)
		munmap(lfb, available);
	if (fd > -1)
		close(fd);
}

void CFbAccelTD::setColor(fb_pixel_t col)
{
	if (col == lastcol)
		return;
	char *c = (char *)&col;
	dfbdest->SetColor(dfbdest, c[1], c[2], c[3], c[0]);
	lastcol = col;
}

void CFbAccelTD::paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	setColor(col);
	dfbdest->FillRectangle(dfbdest, x, y, dx, dy);
}

void CFbAccelTD::paintPixel(const int x, const int y, const fb_pixel_t col)
{
	setColor(col);
	dfbdest->DrawLine(dfbdest, x, y, x, y);
}

void CFbAccelTD::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
	setColor(col);
	dfbdest->DrawLine(dfbdest, xa, ya, xb, yb);
}

void CFbAccelTD::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
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
		fprintf(stderr, LOGTAG "blit2FB: ");
		DirectFBErrorFatal("dfb->CreateSurface(dfb, &dsc, &surf)", err);
	}

	if (transp)
	{
		surf->SetSrcColorKey(surf, 0, 0, 0);
		dfbdest->SetBlittingFlags(dfbdest, DSBLIT_SRC_COLORKEY);
	}
	else
		dfbdest->SetBlittingFlags(dfbdest, DSBLIT_BLEND_ALPHACHANNEL);

	dfbdest->Blit(dfbdest, surf, &src, xoff, yoff);
	surf->Release(surf);
	return;
}

void CFbAccelTD::init(const char *)
{
	CFrameBuffer::init();
	if (lfb == NULL) {
		printf(LOGTAG "CFrameBuffer::init() failed.\n");
		return; /* too bad... */
	}
	available = fix.smem_len;
	printf(LOGTAG "%dk video mem\n", available / 1024);
	memset(lfb, 0, available);

	lbb = lfb;	/* the memory area to draw to... */
	available = fix.smem_len;
	stride = fix.line_length;
	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;

	return;
}

/* wrong name... */
int CFbAccelTD::setMode(unsigned int, unsigned int, unsigned int)
{
	int needmem = stride * yRes * 2;
	if (available >= needmem)
	{
		backbuffer = lfb + stride / sizeof(fb_pixel_t) * yRes;
		return 0;
	}
	fprintf(stderr, LOGTAG " not enough FB memory (have %d, need %d)\n", available, needmem);
	backbuffer = lfb; /* will not work well, but avoid crashes */
	return 0;
}
