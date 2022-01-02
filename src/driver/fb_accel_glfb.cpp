/*
	Framebuffer acceleration hardware abstraction functions.
	This implements stuff needed by the GL Framebuffer.
	Not really accelerated ;-)

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

#include <glfb.h>
extern GLFramebuffer *glfb;

#include <driver/abstime.h>
#include <system/set_threadname.h>
#include <gui/color.h>
#include <gui/color_custom.h>

#define LOGTAG "[fb_glfb] "

CFbAccelGLFB::CFbAccelGLFB()
{
	fb_name = "GL ramebuffer";
}

void CFbAccelGLFB::init(const char *)
{
	int tr = 0xff;

	fd = -1;
	if (!glfb) {
		fprintf(stderr, LOGTAG "init: GL Framebuffer is not set up? we are doomed...\n");
		return;
	}
	screeninfo = glfb->getScreenInfo();
	stride = 4 * screeninfo.xres;
	swidth = screeninfo.xres;
	available = glfb->getOSDBuffer()->size(); /* allocated in glfb constructor */
	lbb = lfb = reinterpret_cast<fb_pixel_t*>(glfb->getOSDBuffer()->data());

	memset(lfb, 0, available);
	setMode(720, 576, 8 * sizeof(fb_pixel_t));

	blit_thread = false;

	/* Google Material Colors */
	paletteSetColor(0x01,			0x010101, tr); // what's this?
	paletteSetColor(COL_WHITE0,		0xFFFFFF, tr);
	paletteSetColor(COL_BLACK0,		0x000000, tr);
	paletteSetColor(COL_DARK_RED0,		0xb71c1c, tr); // red		900
	paletteSetColor(COL_RED0,		0xf44336, tr); // red		500
	paletteSetColor(COL_LIGHT_RED0,		0xe57373, tr); // red		300
	paletteSetColor(COL_DARK_GREEN0,	0x1b5e20, tr); // green		900
	paletteSetColor(COL_GREEN0,		0x4caf50, tr); // green		500
	paletteSetColor(COL_LIGHT_GREEN0,	0x81c784, tr); // green		300
	paletteSetColor(COL_DARK_YELLOW0,	0xf9a825, tr); // yellow	800
	paletteSetColor(COL_YELLOW0,		0xffeb3b, tr); // yellow	500
	paletteSetColor(COL_LIGHT_YELLOW0,	0xfff176, tr); // yellow	300
	paletteSetColor(COL_DARK_BLUE0,		0x1a237e, tr); // indigo	900
	paletteSetColor(COL_BLUE0,		0x3f51b5, tr); // indigo	500
	paletteSetColor(COL_LIGHT_BLUE0,	0x7986cb, tr); // indigo	300
	paletteSetColor(COL_DARK_GRAY0,		0x424242, tr); // grey		800
	paletteSetColor(COL_GRAY0,		0x9e9e9e, tr); // grey		500
	paletteSetColor(COL_LIGHT_GRAY0,	0xe0e0e0, tr); // grey		300

	paletteSetColor(COL_BACKGROUND,		0x000000, 0x0);

	paletteSet();

	useBackground(false);
	m_transparent = m_transparent_default;

	/* start the autoblit-thread (run() function) */
	OpenThreads::Thread::start();
};

CFbAccelGLFB::~CFbAccelGLFB()
{
	if (blit_thread)
	{
		blit_thread = false;
		blit(); /* wakes up the thread */
		OpenThreads::Thread::join();
	}
}

void CFbAccelGLFB::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	CFrameBuffer::blit2FB(fbbuff, width, height, xoff, yoff, xp, yp, transp);
	blit();
}

#define BLIT_INTERVAL_MIN 40
#define BLIT_INTERVAL_MAX 250
void CFbAccelGLFB::run()
{
	printf(LOGTAG "run start\n");
	int64_t last_blit = 0;
	blit_pending = false;
	blit_thread = true;
	blit_mutex.lock();
	set_threadname("glfb::autoblit");
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

void CFbAccelGLFB::blit()
{
	//printf(LOGTAG "blit\n");
	blit_mutex.lock();
	blit_cond.signal();
	blit_mutex.unlock();
}

void CFbAccelGLFB::_blit()
{
	if (glfb)
		glfb->blit();
}

/* wrong name... */
int CFbAccelGLFB::setMode(unsigned int, unsigned int, unsigned int)
{
	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;
	int needmem = stride * yRes * 2;
	if (available >= needmem)
	{
		backbuffer = lfb + swidth * yRes;
		return 0;
	}
	fprintf(stderr, LOGTAG " not enough FB memory (have %d, need %d)\n", available, needmem);
	backbuffer = lfb; /* will not work well, but avoid crashes */
	return 0;
}

fb_pixel_t * CFbAccelGLFB::getBackBufferPointer() const
{
	return backbuffer;
}
