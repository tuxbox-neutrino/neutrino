/*
	Copyright (C) 2007-2013 Stefan Seyfried

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

	private functions for the fbaccel class (only used in CFrameBuffer)
*/


#ifndef __fbaccel__
#define __fbaccel__
#include <config.h>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include "fb_generic.h"

#if HAVE_SPARK_HARDWARE
#define PARTIAL_BLIT 1
#endif

#if HAVE_COOL_HARDWARE
#ifndef BOXMODEL_APOLLO
/* not needed -- if you don't want acceleration, don't call CFbAccel ;) */
#define USE_NEVIS_GXA 1
#endif
#endif

class CFbAccel
	: public CFrameBuffer
{
	public:
		CFbAccel();
		~CFbAccel();
		void paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius, int type);
		virtual void paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col);
};

class CFbAccelSTi
	: public OpenThreads::Thread, public CFbAccel
{
	private:
		void run(void);
		void blit(void);
		void _blit(void);
		bool blit_thread;
		bool blit_pending;
		OpenThreads::Condition blit_cond;
		OpenThreads::Mutex blit_mutex;
		fb_pixel_t *backbuffer;
#ifdef PARTIAL_BLIT
		struct {
			int xs;
			int ys;
			int xe;
			int ye;
		} to_blit;
		uint32_t last_xres;
#endif
	public:
		CFbAccelSTi();
		~CFbAccelSTi();
		void init(const char * const);
		int setMode(unsigned int xRes, unsigned int yRes, unsigned int bpp);
		void paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col);
		void blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp);
		void waitForIdle(const char *func = NULL);
		void mark(int x, int y, int dx, int dy);
		fb_pixel_t * getBackBufferPointer() const;
		void setBlendMode(uint8_t);
		void setBlendLevel(int);
};

class CFbAccelCSNevis
	: public CFbAccel
{
	private:
		fb_pixel_t lastcol;
		int		  devmem_fd;	/* to access the GXA register we use /dev/mem */
		unsigned int	  smem_start;	/* as aquired from the fbdev, the framebuffers physical start address */
		volatile uint8_t *gxa_base;	/* base address for the GXA's register access */
		void setColor(fb_pixel_t col);
		void run(void);
		fb_pixel_t *backbuffer;
	public:
		CFbAccelCSNevis();
		~CFbAccelCSNevis();
		void init(const char * const);
		int setMode(unsigned int xRes, unsigned int yRes, unsigned int bpp);
		void paintPixel(int x, int y, const fb_pixel_t col);
		void paintRect(const int x, const int y, const int dx, const int dy, const fb_pixel_t col);
		void paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col);
		void paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius = 0, int type = CORNER_ALL);
		void blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp = 0, uint32_t yp = 0, bool transp = false);
		void blitBox2FB(const fb_pixel_t* boxBuf, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);
		void waitForIdle(const char *func = NULL);
		fb_pixel_t * getBackBufferPointer() const;
		void setBlendMode(uint8_t);
		void setBlendLevel(int);
		void add_gxa_sync_marker(void);
		void setupGXA(void);
};

class CFbAccelCSApollo
	: public CFbAccel
{
	private:
		fb_pixel_t *backbuffer;

	public:
		CFbAccelCSApollo();
//		~CFbAccelCSApollo();
		int setMode(unsigned int xRes, unsigned int yRes, unsigned int bpp);

		void paintHLineRelInternal(int x, int dx, int y, const fb_pixel_t col);
		void paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius = 0, int type = CORNER_ALL);
		void blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp = 0, uint32_t yp = 0, bool transp = false);
		void blitBox2FB(const fb_pixel_t* boxBuf, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);
		void setBlendMode(uint8_t);
		void setBlendLevel(int);
};

#endif
