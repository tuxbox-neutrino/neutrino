/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
                      2003 thegoodguy
	Copyright (C) 2007-2015 Stefan Seyfried

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

#include <driver/framebuffer_ng.h>
#include <driver/fbaccel.h>

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>
#include <math.h>

#include <linux/kd.h>

#include <gui/audiomute.h>
#include <gui/color.h>
#include <gui/pictureviewer.h>
#include <system/debug.h>
#include <global.h>
#include <video.h>
#include <cs_api.h>
#ifdef HAVE_COOL_HARDWARE
#include <cnxtfb.h>
#endif
#if HAVE_TRIPLEDRAGON
#ifdef SCALE
#undef SCALE
#endif
#include <tdgfx/stb04gfx.h>
extern int gfxfd;
#endif

extern CPictureViewer * g_PicViewer;
#define ICON_CACHE_SIZE 1024*1024*2 // 2mb

#define BACKGROUNDIMAGEWIDTH 720

/* note that it is *not* enough to just change those values */
#define DEFAULT_XRES 1280
#define DEFAULT_YRES 720
#define DEFAULT_BPP  32

/*******************************************************************************/

void CFrameBuffer::waitForIdle(const char *)
{
	accel->waitForIdle();
}

/*******************************************************************************/

static uint8_t * virtual_fb = NULL;
inline unsigned int make16color(uint16_t r, uint16_t g, uint16_t b, uint16_t t,
                                  uint32_t  /*rl*/ = 0, uint32_t  /*ro*/ = 0,
                                  uint32_t  /*gl*/ = 0, uint32_t  /*go*/ = 0,
                                  uint32_t  /*bl*/ = 0, uint32_t  /*bo*/ = 0,
                                  uint32_t  /*tl*/ = 0, uint32_t  /*to*/ = 0)
{
        return ((t << 24) & 0xFF000000) | ((r << 8) & 0xFF0000) | ((g << 0) & 0xFF00) | (b >> 8 & 0xFF);
}

/* this table contains the x coordinates for a quarter circle (the bottom right quarter) with fixed
   radius of 540 px which is the half of the max HD graphics size of 1080 px. So with that table we
    ca draw boxes with round corners and als circles by just setting dx = dy = radius (max 540). */
static const int q_circle[541] = {
	540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540, 540,
	540, 540, 540, 540, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539,
	539, 538, 538, 538, 538, 538, 538, 538, 538, 538, 538, 538, 538, 537, 537, 537, 537, 537, 537, 537,
	537, 537, 536, 536, 536, 536, 536, 536, 536, 536, 535, 535, 535, 535, 535, 535, 535, 535, 534, 534,
	534, 534, 534, 534, 533, 533, 533, 533, 533, 533, 532, 532, 532, 532, 532, 532, 531, 531, 531, 531,
	531, 531, 530, 530, 530, 530, 529, 529, 529, 529, 529, 529, 528, 528, 528, 528, 527, 527, 527, 527,
	527, 526, 526, 526, 526, 525, 525, 525, 525, 524, 524, 524, 524, 523, 523, 523, 523, 522, 522, 522,
	522, 521, 521, 521, 521, 520, 520, 520, 519, 519, 519, 518, 518, 518, 518, 517, 517, 517, 516, 516,
	516, 515, 515, 515, 515, 514, 514, 514, 513, 513, 513, 512, 512, 512, 511, 511, 511, 510, 510, 510,
	509, 509, 508, 508, 508, 507, 507, 507, 506, 506, 506, 505, 505, 504, 504, 504, 503, 503, 502, 502,
	502, 501, 501, 500, 500, 499, 499, 499, 498, 498, 498, 497, 497, 496, 496, 496, 495, 495, 494, 494,
	493, 493, 492, 492, 491, 491, 490, 490, 490, 489, 489, 488, 488, 487, 487, 486, 486, 485, 485, 484,
	484, 483, 483, 482, 482, 481, 481, 480, 480, 479, 479, 478, 478, 477, 477, 476, 476, 475, 475, 474,
	473, 473, 472, 472, 471, 471, 470, 470, 469, 468, 468, 467, 466, 466, 465, 465, 464, 464, 463, 462,
	462, 461, 460, 460, 459, 459, 458, 458, 457, 456, 455, 455, 454, 454, 453, 452, 452, 451, 450, 450,
	449, 449, 448, 447, 446, 446, 445, 445, 444, 443, 442, 441, 441, 440, 440, 439, 438, 437, 436, 436,
	435, 435, 434, 433, 432, 431, 431, 430, 429, 428, 427, 427, 426, 425, 425, 424, 423, 422, 421, 421,
	420, 419, 418, 417, 416, 416, 415, 414, 413, 412, 412, 411, 410, 409, 408, 407, 406, 405, 404, 403,
	403, 402, 401, 400, 399, 398, 397, 397, 395, 394, 393, 393, 392, 391, 390, 389, 388, 387, 386, 385,
	384, 383, 382, 381, 380, 379, 378, 377, 376, 375, 374, 373, 372, 371, 369, 368, 367, 367, 365, 364,
	363, 362, 361, 360, 358, 357, 356, 355, 354, 353, 352, 351, 350, 348, 347, 346, 345, 343, 342, 341,
	340, 339, 337, 336, 335, 334, 332, 331, 329, 328, 327, 326, 324, 323, 322, 321, 319, 317, 316, 315,
	314, 312, 310, 309, 308, 307, 305, 303, 302, 301, 299, 297, 296, 294, 293, 291, 289, 288, 287, 285,
	283, 281, 280, 278, 277, 275, 273, 271, 270, 268, 267, 265, 263, 261, 259, 258, 256, 254, 252, 250,
	248, 246, 244, 242, 240, 238, 236, 234, 232, 230, 228, 225, 223, 221, 219, 217, 215, 212, 210, 207,
	204, 202, 200, 197, 195, 192, 190, 187, 184, 181, 179, 176, 173, 170, 167, 164, 160, 157, 154, 150,
	147, 144, 140, 136, 132, 128, 124, 120, 115, 111, 105, 101,  95,  89,  83,  77,  69,  61,  52,  40,
	 23};

static inline bool calcCorners(int *ofs, int *ofl, int *ofr, const int& dy, const int& line, const int& radius,
				const bool& tl, const bool& tr, const bool& bl, const bool& br)
{
/* just a multiplicator for all math to reduce rounding errors */
#define MUL 32768
	int scl, _ofs = 0;
	bool ret = false;
	if (ofl != NULL) *ofl = 0;
	if (ofr != NULL) *ofr = 0;
	int scf = 540 * MUL / radius;
	/* one of the top corners */
	if (line < radius && (tl || tr)) {
		/* uper round corners */
		scl = scf * (radius - line) / MUL;
		if ((scf * (radius - line) % MUL) >= (MUL / 2)) /* round up */
			scl++;
		_ofs =  radius - (q_circle[scl] * MUL / scf);
		if (ofl != NULL && tl) *ofl = _ofs;
		if (ofr != NULL && tr) *ofr = _ofs;
	}
	/* one of the bottom corners */
	else if ((line >= dy - radius) && (bl || br)) {
		/* lower round corners */
		scl = scf * (radius - (dy - (line + 1))) / MUL;
		if ((scf * (radius - (dy - (line + 1))) % MUL) >= (MUL / 2)) /* round up */
			scl++;
		_ofs =  radius - (q_circle[scl] * MUL / scf);
		if (ofl != NULL && bl) *ofl = _ofs;
		if (ofr != NULL && br) *ofr = _ofs;
	}
	else
		ret = true;
	if (ofs != NULL) *ofs = _ofs;
	return ret;
}

static inline int limitRadius(const int& dx, const int& dy, const int& radius)
{
	int m = std::min(dx, dy);
	if (radius > m)
		return m;
	if (radius > 540)
		return 540;
	return radius;
}

CFrameBuffer::CFrameBuffer()
: active ( true )
{
	iconBasePath = "";
	available  = 0;
	cmap.start = 0;
	cmap.len = 256;
	cmap.red = red;
	cmap.green = green;
	cmap.blue  = blue;
	cmap.transp = trans;
	backgroundColor = 0;
	useBackgroundPaint = false;
	background = NULL;
	backupBackground = NULL;
	backgroundFilename = "";
	fbAreaActiv = false;
	fb_no_check = false;
	fd  = 0;
	tty = 0;
	bpp = 0;
	locked = false;
	m_transparent_default = CFrameBuffer::TM_BLACK; // TM_BLACK: Transparency when black content ('pseudo' transparency)
							// TM_NONE:  No 'pseudo' transparency
							// TM_INI:   Transparency depends on g_settings.infobar_alpha ???
	m_transparent         = m_transparent_default;
//FIXME: test
	memset(red, 0, 256*sizeof(__u16));
	memset(green, 0, 256*sizeof(__u16));
	memset(blue, 0, 256*sizeof(__u16));
	memset(trans, 0, 256*sizeof(__u16));
}

CFrameBuffer* CFrameBuffer::getInstance()
{
	static CFrameBuffer* frameBuffer = NULL;

	if(!frameBuffer) {
		frameBuffer = new CFrameBuffer();
		printf("[neutrino] frameBuffer Instance created\n");
	} else {
		//printf("[neutrino] frameBuffer Instace requested\n");
	}
	return frameBuffer;
}

#ifdef USE_NEVIS_GXA
void CFrameBuffer::setupGXA(void)
{
	accel->setupGXA();
}
#endif

void CFrameBuffer::init(const char * const)
{
        int tr = 0xFF;
	accel = new CFbAccel(this);
	cache_size = 0;

        /* Windows Colors */
        paletteSetColor(0x1, 0x010101, tr);
        paletteSetColor(0x2, 0x800000, tr);
        paletteSetColor(0x3, 0x008000, tr);
        paletteSetColor(0x4, 0x808000, tr);
        paletteSetColor(0x5, 0x000080, tr);
        paletteSetColor(0x6, 0x800080, tr);
        paletteSetColor(0x7, 0x008080, tr);
        paletteSetColor(0x8, 0xA0A0A0, tr);
        paletteSetColor(0x9, 0x505050, tr);
        paletteSetColor(0xA, 0xFF0000, tr);
        paletteSetColor(0xB, 0x00FF00, tr);
        paletteSetColor(0xC, 0xFFFF00, tr);
        paletteSetColor(0xD, 0x0000FF, tr);
        paletteSetColor(0xE, 0xFF00FF, tr);
        paletteSetColor(0xF, 0x00FFFF, tr);
        paletteSetColor(0x10, 0xFFFFFF, tr);
        paletteSetColor(0x11, 0x000000, tr);
	paletteSetColor(COL_BACKGROUND, 0x000000, 0x0);

        paletteSet();

        useBackground(false);
	m_transparent = m_transparent_default;
	return;
}


CFrameBuffer::~CFrameBuffer()
{
	active = false; /* keep people/infoclocks from accessing */
	std::map<std::string, rawIcon>::iterator it;

	for(it = icon_cache.begin(); it != icon_cache.end(); ++it) {
		/* printf("FB: delete cached icon %s: %x\n", it->first.c_str(), (int) it->second.data); */
		cs_free_uncached(it->second.data);
	}
	icon_cache.clear();

	if (background) {
		delete[] background;
		background = NULL;
	}

	if (backupBackground) {
		delete[] backupBackground;
		backupBackground = NULL;
	}

	if (virtual_fb){
		delete[] virtual_fb;
		virtual_fb = NULL;
	}
	delete accel;
}

int CFrameBuffer::getFileHandle() const
{
	fprintf(stderr, "[fb]::%s: WARNING, this should never be used, please report!\n", __func__);
	return fd;
}

unsigned int CFrameBuffer::getStride() const
{
	return stride;
}

unsigned int CFrameBuffer::getScreenWidth(bool real)
{
	if(real)
		return xRes;
	else
		return g_settings.screen_EndX - g_settings.screen_StartX;
}

unsigned int CFrameBuffer::getScreenHeight(bool real)
{
	if(real)
		return yRes;
	else
		return g_settings.screen_EndY - g_settings.screen_StartY;
}

unsigned int CFrameBuffer::getScreenWidthRel(bool force_small)
{
	int percent = force_small ? WINDOW_SIZE_MIN_FORCED : g_settings.window_width;
	// always reduce a possible detailline
	return (g_settings.screen_EndX - g_settings.screen_StartX - 2*ConnectLineBox_Width) * percent / 100;
}

unsigned int CFrameBuffer::getScreenHeightRel(bool force_small)
{
	int percent = force_small ? WINDOW_SIZE_MIN_FORCED : g_settings.window_height;
	return (g_settings.screen_EndY - g_settings.screen_StartY) * percent / 100;
}

unsigned int CFrameBuffer::getScreenX()
{
	return g_settings.screen_StartX;
}

unsigned int CFrameBuffer::getScreenY()
{
	return g_settings.screen_StartY;
}

fb_pixel_t * CFrameBuffer::getFrameBufferPointer() const
{
	if (active || (virtual_fb == NULL))
		return accel->lbb;
	else
		return (fb_pixel_t *)virtual_fb;
}

fb_pixel_t * CFrameBuffer::getBackBufferPointer() const
{
	return accel->backbuffer;
}

bool CFrameBuffer::getActive() const
{
	return (active || (virtual_fb != NULL));
}

void CFrameBuffer::setActive(bool enable)
{
	active = enable;
}

t_fb_var_screeninfo *CFrameBuffer::getScreenInfo()
{
	return &screeninfo;
}

int CFrameBuffer::setMode(unsigned int /*nxRes*/, unsigned int /*nyRes*/, unsigned int /*nbpp*/)
{
fprintf(stderr, "CFrameBuffer::setMode avail: %d active: %d\n", available, active);
	if (!available&&!active)
		return -1;

	int ret = accel->setMode();
	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;
	printf("FB: %dx%dx%d line length %d. %s nevis GXA accelerator.\n", xRes, yRes, bpp, stride,
#ifdef USE_NEVIS_GXA
		"Using"
#else
		"Not using"
#endif
	);
	accel->update(); /* just in case we need to update stuff */

	//memset(getFrameBufferPointer(), 0, stride * yRes);
	paintBackground();
	return ret;
}
#if 0 
//never used
void CFrameBuffer::setTransparency( int /*tr*/ )
{
}
#endif

#if !HAVE_TRIPLEDRAGON
void CFrameBuffer::setBlendMode(uint8_t mode)
{
	(void)mode;
#ifdef HAVE_COOL_HARDWARE
	if (ioctl(fd, FBIO_SETBLENDMODE, mode))
		printf("FBIO_SETBLENDMODE failed.\n");
#endif
}

void CFrameBuffer::setBlendLevel(int level)
{
	(void)level;
#ifdef HAVE_COOL_HARDWARE
	//printf("CFrameBuffer::setBlendLevel %d\n", level);
	unsigned char value = 0xFF;
	if((level >= 0) && (level <= 100))
		value = convertSetupAlpha2Alpha(level);

	if (ioctl(fd, FBIO_SETOPACITY, value))
		printf("FBIO_SETOPACITY failed.\n");
#if 1
       if(level == 100) // TODO: sucks.
               usleep(20000);
#endif
#endif
}
#else
/* TRIPLEDRAGON */
void CFrameBuffer::setBlendMode(uint8_t mode)
{
	Stb04GFXOsdControl g;
	ioctl(gfxfd, STB04GFX_OSD_GETCONTROL, &g);
	g.use_global_alpha = (mode == 2); /* 1 == pixel alpha, 2 == global alpha */
	ioctl(gfxfd, STB04GFX_OSD_SETCONTROL, &g);
}

void CFrameBuffer::setBlendLevel(int level)
{
	/* this is bypassing directfb, but faster and easier */
	Stb04GFXOsdControl g;
	ioctl(gfxfd, STB04GFX_OSD_GETCONTROL, &g);
	if (g.use_global_alpha == 0)
		return;

	if (level < 0 || level > 100)
		return;

	/* this is the same as convertSetupAlpha2Alpha(), but non-float */
	g.global_alpha = 255 - (255 * level / 100);
	ioctl(gfxfd, STB04GFX_OSD_SETCONTROL, &g);
	if (level == 100) // sucks
		usleep(20000);
}
#endif

#if 0 
//never used
void CFrameBuffer::setAlphaFade(int in, int num, int tr)
{
	for (int i=0; i<num; i++) {
		cmap.transp[in+i]=tr;
	}
}
#endif
void CFrameBuffer::paletteFade(int i, __u32 rgb1, __u32 rgb2, int level)
{
	__u16 *r = cmap.red+i;
	__u16 *g = cmap.green+i;
	__u16 *b = cmap.blue+i;
	*r= ((rgb2&0xFF0000)>>16)*level;
	*g= ((rgb2&0x00FF00)>>8 )*level;
	*b= ((rgb2&0x0000FF)    )*level;
	*r+=((rgb1&0xFF0000)>>16)*(255-level);
	*g+=((rgb1&0x00FF00)>>8 )*(255-level);
	*b+=((rgb1&0x0000FF)    )*(255-level);
}

void CFrameBuffer::paletteGenFade(int in, __u32 rgb1, __u32 rgb2, int num, int tr)
{
	for (int i=0; i<num; i++) {
		paletteFade(in+i, rgb1, rgb2, i*(255/(num-1)));
		cmap.transp[in+i]=tr;
		tr--; //FIXME
	}
}

void CFrameBuffer::paletteSetColor(int i, __u32 rgb, int tr)
{
	cmap.red[i]    =(rgb&0xFF0000)>>8;
	cmap.green[i]  =(rgb&0x00FF00)   ;
	cmap.blue[i]   =(rgb&0x0000FF)<<8;
	cmap.transp[i] = tr;
}

void CFrameBuffer::paletteSet(struct fb_cmap *map)
{
	if (!active)
		return;

	if(map == NULL)
		map = &cmap;

	if(bpp == 8) {
//printf("Set palette for %dbit\n", bpp);
		ioctl(fd, FBIOPUTCMAP, map);
	}
	uint32_t  rl, ro, gl, go, bl, bo, tl, to;
        rl = screeninfo.red.length;
        ro = screeninfo.red.offset;
        gl = screeninfo.green.length;
        go = screeninfo.green.offset;
        bl = screeninfo.blue.length;
        bo = screeninfo.blue.offset;
        tl = screeninfo.transp.length;
        to = screeninfo.transp.offset;
	for (int i = 0; i < 256; i++) {
                realcolor[i] = make16color(cmap.red[i], cmap.green[i], cmap.blue[i], cmap.transp[i],
                                           rl, ro, gl, go, bl, bo, tl, to);
	}
	OnAfterSetPallette();
}

void CFrameBuffer::paintHLineRelInternal2Buf(const int& x, const int& dx, const int& y, const int& box_dx, const fb_pixel_t& col, fb_pixel_t* buf)
{
	uint8_t * pos = ((uint8_t *)buf) + x * sizeof(fb_pixel_t) + box_dx * sizeof(fb_pixel_t) * y;
	fb_pixel_t * dest = (fb_pixel_t *)pos;
	for (int i = 0; i < dx; i++)
		*(dest++) = col;
}

fb_pixel_t* CFrameBuffer::paintBoxRel2Buf(const int dx, const int dy, const fb_pixel_t col, fb_pixel_t* buf/* = NULL*/, int radius/* = 0*/, int type/* = CORNER_ALL*/)
{
	if (!getActive())
		return buf;
	if (dx == 0 || dy == 0) {
		dprintf(DEBUG_INFO, "[%s - %d]: radius %d, dx %d dy %d\n", __func__, __LINE__, radius, dx, dy);
		return buf;
	}

	fb_pixel_t* pixBuf = buf;
	if (pixBuf == NULL) {
		pixBuf = (fb_pixel_t*) cs_malloc_uncached(dx*dy*sizeof(fb_pixel_t));
		if (pixBuf == NULL) {
			dprintf(DEBUG_NORMAL, "[%s #%d] Error cs_malloc_uncached\n", __func__, __LINE__);
			return NULL;
		}
	}
	memset((void*)pixBuf, '\0', dx*dy*sizeof(fb_pixel_t));

	if (type && radius) {
		//setCornerFlags(type);
		radius = limitRadius(dx, dy, radius);

		bool corner_tl = !!(type & CORNER_TOP_LEFT);
		bool corner_tr = !!(type & CORNER_TOP_RIGHT);
		bool corner_bl = !!(type & CORNER_BOTTOM_LEFT);
		bool corner_br = !!(type & CORNER_BOTTOM_RIGHT);

		int line = 0;
		while (line < dy) {
			int ofl, ofr;
			calcCorners(NULL, &ofl, &ofr, dy, line, radius,
				    corner_tl, corner_tr, corner_bl, corner_br);
			if (dx-ofr-ofl < 1) {
				if (dx-ofr-ofl == 0) {
					dprintf(DEBUG_INFO, "[%s - %d]: radius %d, end x %d y %d\n", __func__, __LINE__, radius, dx-ofr-ofl, line);
				}
				else {
					dprintf(DEBUG_INFO, "[%s - %04d]: Calculated width: %d\n		      (radius %d, dx %d, offsetLeft %d, offsetRight %d).\n		      Width can not be less than 0, abort.\n",
					       __func__, __LINE__, dx-ofr-ofl, radius, dx, ofl, ofr);
				}
				line++;
				continue;
			}
			paintHLineRelInternal2Buf(ofl, dx-ofl-ofr, line, dx, col, pixBuf);
			line++;
		}
	} else {
		fb_pixel_t *bp = pixBuf;
		int line = 0;
		while (line < dy) {
			for (int pos = 0; pos < dx; pos++)
				*(bp + pos) = col;
			bp += dx;
			line++;
		}
	}
	return pixBuf;
}

fb_pixel_t* CFrameBuffer::paintBoxRel(const int x, const int y, const int dx, const int dy,
				      const fb_pixel_t /*col*/, gradientData_t *gradientData,
				      int radius, int type)
{
#define MASK 0xFFFFFFFF

	fb_pixel_t* boxBuf    = paintBoxRel2Buf(dx, dy, MASK, NULL, radius, type);
	fb_pixel_t *bp        = boxBuf;
	fb_pixel_t *gra       = gradientData->gradientBuf;
	gradientData->boxBuf  = boxBuf;
	gradientData->x       = x;
	gradientData->dx      = dx;

	if (gradientData->direction == gradientVertical) {
		// vertical
		for (int pos = 0; pos < dx; pos++) {
			for(int count = 0; count < dy; count++) {
				if (*(bp + pos) == MASK)
					*(bp + pos) = (fb_pixel_t)(*(gra + count));
				bp += dx;
			}
			bp = boxBuf;
		}
	} else {
		// horizontal
		for (int line = 0; line < dy; line++) {
			for (int pos = 0; pos < dx; pos++) {
				if (*(bp + pos) == MASK)
					*(bp + pos) = (fb_pixel_t)(*(gra + pos));
			}
			bp += dx;
		}
	}

	if ((gradientData->mode & pbrg_noPaint) == pbrg_noPaint)
		return boxBuf;

//	blit2FB(boxBuf, dx, dy, x, y);
	blitBox2FB(boxBuf, dx, dy, x, y);

	if ((gradientData->mode & pbrg_noFree) == pbrg_noFree)
		return boxBuf;

	cs_free_uncached(boxBuf);

	return NULL;
}

void CFrameBuffer::paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius, int type)
{
	/* draw a filled rectangle (with additional round corners) */
	if (!getActive())
		return;

	bool corner_tl = !!(type & CORNER_TOP_LEFT);
	bool corner_tr = !!(type & CORNER_TOP_RIGHT);
	bool corner_bl = !!(type & CORNER_BOTTOM_LEFT);
	bool corner_br = !!(type & CORNER_BOTTOM_RIGHT);

	checkFbArea(x, y, dx, dy, true);

	if (!type || !radius)
	{
		accel->paintRect(x, y, dx, dy, col);
		checkFbArea(x, y, dx, dy, false);
		return;
	}

	/* limit the radius */
	radius = limitRadius(dx, dy, radius);
	if (radius < 1)		/* dx or dy = 0... */
		radius = 1;	/* avoid div by zero below */

	int line = 0;
	while (line < dy) {
		int ofl, ofr;
		if (calcCorners(NULL, &ofl, &ofr, dy, line, radius,
				corner_tl, corner_tr, corner_bl, corner_br))
		{
			int height = dy - ((corner_tl || corner_tr)?radius: 0 ) - ((corner_bl || corner_br) ? radius : 0);
			accel->paintRect(x, y + line, dx, height, col);
			line += height;
			continue;
		}
		if (dx - ofr - ofl < 1) {
			//printf("FB-NG::%s:%d x %d y %d dx %d dy %d l %d r %d\n", __func__, __LINE__, x,y,dx,dy, ofl, ofr);
			line++;
			continue;
		}
		accel->paintLine(x + ofl, y + line, x + dx - ofr, y + line, col);
		line++;
	}
	checkFbArea(x, y, dx, dy, false);
	accel->mark(x, y, x+dx, y+dy);
}

void CFrameBuffer::paintPixel(const int x, const int y, const fb_pixel_t col)
{
	if (!getActive())
		return;
	if (x > (int)xRes || y > (int)yRes || x < 0 || y < 0)
		return;

	accel->paintPixel(x, y, col);
}

void CFrameBuffer::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
	if (!getActive())
		return;

	accel->paintLine(xa, ya, xb, yb, col);
}

void CFrameBuffer::paintVLine(int x, int ya, int yb, const fb_pixel_t col)
{
	paintLine(x, ya, x, yb, col);
}

void CFrameBuffer::paintVLineRel(int x, int y, int dy, const fb_pixel_t col)
{
	paintLine(x, y, x, y + dy, col);
}

void CFrameBuffer::paintHLine(int xa, int xb, int y, const fb_pixel_t col)
{
	paintLine(xa, y, xb, y, col);
}

void CFrameBuffer::paintHLineRel(int x, int dx, int y, const fb_pixel_t col)
{
	paintLine(x, y, x + dx, y, col);
}

void CFrameBuffer::setIconBasePath(const std::string & iconPath)
{
	iconBasePath = iconPath;
	if (!iconBasePath.empty() && '/' == *iconBasePath.rbegin()) /* .back() is only c++11 :-( */
		return;
	iconBasePath += "/";
}

std::string CFrameBuffer::getIconPath(std::string icon_name, std::string file_type)
{
	std::string path, filetype;
	filetype = "." + file_type;
	path = std::string(ICONSDIR_VAR) + "/" + icon_name + filetype;
	if (access(path.c_str(), F_OK))
		path = iconBasePath + "/" + icon_name + filetype;
	if (icon_name.find("/", 0) != std::string::npos)
		path = icon_name;
	return path;
}

void CFrameBuffer::getIconSize(const char * const filename, int* width, int *height)
{
	*width = 0;
	*height = 0;

	if(filename == NULL)
		return;

	std::map<std::string, rawIcon>::iterator it;

	/* if code ask for size, lets cache it. assume we have enough ram for cache */
	/* FIXME offset seems never used in code, always default = 1 ? */

	it = icon_cache.find(filename);
	if(it == icon_cache.end()) {
		if(paintIcon(filename, 0, 0, 0, 1, false)) {
			it = icon_cache.find(filename);
		}
	}
	if(it != icon_cache.end()) {
		*width = it->second.width;
		*height = it->second.height;
	}
}

bool CFrameBuffer::paintIcon8(const std::string & filename, const int x, const int y, const unsigned char offset)
{
	if (!getActive())
		return false;

//printf("%s(file, %d, %d, %d)\n", __FUNCTION__, x, y, offset);

	struct rawHeader header;
	uint16_t         width, height;
	int              lfd;

	lfd = open((iconBasePath + filename).c_str(), O_RDONLY);

	if (lfd == -1) {
		printf("paintIcon8: error while loading icon: %s%s\n", iconBasePath.c_str(), filename.c_str());
		return false;
	}

	read(lfd, &header, sizeof(struct rawHeader));

	width  = (header.width_hi  << 8) | header.width_lo;
	height = (header.height_hi << 8) | header.height_lo;

	unsigned char pixbuf[768];

	uint8_t * d = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * d2;
	for (int count=0; count<height; count ++ ) {
		read(lfd, &pixbuf[0], width );
		unsigned char *pixpos = &pixbuf[0];
		d2 = (fb_pixel_t *) d;
		for (int count2=0; count2<width; count2 ++ ) {
			unsigned char color = *pixpos;
			if (color != header.transp) {
//printf("icon8: col %d transp %d real %08X\n", color+offset, header.transp, realcolor[color+offset]);
				paintPixel(d2, color + offset);
			}
			d2++;
			pixpos++;
		}
		d += stride;
	}
	close(lfd);
	accel->mark(x, y, x + width, y + height);
	return true;
}

/* paint icon at position x/y,
   if height h is given, center vertically between y and y+h
   offset is a color offset (probably only useful with palette) */
bool CFrameBuffer::paintIcon(const std::string & filename, const int x, const int y,
			     const int h, const unsigned char offset, bool paint, bool paintBg, const fb_pixel_t colBg)
{
	struct rawHeader header;
	int         width, height;
	int              lfd;
	fb_pixel_t * data;
	struct rawIcon tmpIcon;
	std::map<std::string, rawIcon>::iterator it;
	int dsize;

	if (!getActive())
		return false;

	int  yy = y;
	//printf("CFrameBuffer::paintIcon: load %s\n", filename.c_str());fflush(stdout);

	if (filename.empty())
		return false; /* nothing to do */
	/* we cache and check original name */
	it = icon_cache.find(filename);
	if(it == icon_cache.end()) {
		std::string newname = getIconPath(filename);
		//printf("CFrameBuffer::paintIcon: check for %s\n", newname.c_str());fflush(stdout);

		data = g_PicViewer->getIcon(newname, &width, &height);

		if(data) {
			dsize = width*height*sizeof(fb_pixel_t);
			//printf("CFrameBuffer::paintIcon: %s found, data %x size %d x %d\n", newname.c_str(), data, width, height);fflush(stdout);
			if(cache_size+dsize < ICON_CACHE_SIZE) {
				cache_size += dsize;
				tmpIcon.width = width;
				tmpIcon.height = height;
				tmpIcon.data = data;
				icon_cache.insert(std::pair <std::string, rawIcon> (filename, tmpIcon));
				//printf("Cached %s, cache size %d\n", newname.c_str(), cache_size);
			}
			goto _display;
		}

		newname = getIconPath(filename, "raw");

		lfd = open(newname.c_str(), O_RDONLY);

		if (lfd == -1) {
			//printf("paintIcon: error while loading icon: %s\n", newname.c_str());
			return false;
		}
		read(lfd, &header, sizeof(struct rawHeader));

		tmpIcon.width = width  = (header.width_hi  << 8) | header.width_lo;
		tmpIcon.height = height = (header.height_hi << 8) | header.height_lo;

		dsize = width*height*sizeof(fb_pixel_t);

		tmpIcon.data = (fb_pixel_t*) cs_malloc_uncached(dsize);
		data = tmpIcon.data;

		unsigned char pixbuf[768];
		for (int count = 0; count < height; count ++ ) {
			read(lfd, &pixbuf[0], width >> 1 );
			unsigned char *pixpos = &pixbuf[0];
			for (int count2 = 0; count2 < width >> 1; count2 ++ ) {
				unsigned char compressed = *pixpos;
				unsigned char pix1 = (compressed & 0xf0) >> 4;
				unsigned char pix2 = (compressed & 0x0f);
				if (pix1 != header.transp)
					*data++ = realcolor[pix1+offset];
				else
					*data++ = 0;
				if (pix2 != header.transp)
					*data++ = realcolor[pix2+offset];
				else
					*data++ = 0;
				pixpos++;
			}
		}
		close(lfd);

		data = tmpIcon.data;

		if(cache_size+dsize < ICON_CACHE_SIZE) {
			cache_size += dsize;
			icon_cache.insert(std::pair <std::string, rawIcon> (filename, tmpIcon));
			//printf("Cached %s, cache size %d\n", newname.c_str(), cache_size);
		}
	} else {
		data = it->second.data;
		width = it->second.width;
		height = it->second.height;
		//printf("paintIcon: already cached %s %d x %d\n", newname.c_str(), width, height);
	}
_display:
	if(!paint)
		return true;

	if (h != 0)
		yy += (h - height) / 2;

	checkFbArea(x, yy, width, height, true);
	if (paintBg)
		paintBoxRel(x, yy, width, height, colBg);
	blit2FB(data, width, height, x, yy);
	checkFbArea(x, yy, width, height, false);
	return true;
 
}

void CFrameBuffer::loadPal(const std::string & filename, const unsigned char offset, const unsigned char endidx)
{
	if (!getActive())
		return;

//printf("%s()\n", __FUNCTION__);

	struct rgbData rgbdata;
	int            lfd;

	lfd = open((iconBasePath + filename).c_str(), O_RDONLY);

	if (lfd == -1) {
		printf("error while loading palette: %s%s\n", iconBasePath.c_str(), filename.c_str());
		return;
	}

	int pos = 0;
	int readb = read(lfd, &rgbdata,  sizeof(rgbdata) );
	while(readb) {
		__u32 rgb = (rgbdata.r<<16) | (rgbdata.g<<8) | (rgbdata.b);
		int colpos = offset+pos;
		if( colpos>endidx)
			break;

		paletteSetColor(colpos, rgb, 0xFF);
		readb = read(lfd, &rgbdata,  sizeof(rgbdata) );
		pos++;
	}
	paletteSet(&cmap);
	close(lfd);
}


void CFrameBuffer::paintBoxFrame(const int sx, const int sy, const int dx, const int dy, const int px, const fb_pixel_t col, const int rad, int type)
{
	if (!getActive())
		return;

	int radius = rad;
	bool corner_tl = !!(type & CORNER_TOP_LEFT);
	bool corner_tr = !!(type & CORNER_TOP_RIGHT);
	bool corner_bl = !!(type & CORNER_BOTTOM_LEFT);
	bool corner_br = !!(type & CORNER_BOTTOM_RIGHT);

	int r_tl = 0, r_tr = 0, r_bl = 0, r_br = 0;
	if (type && radius) {
		int x_rad = radius - 1;
		if (corner_tl) r_tl = x_rad;
		if (corner_tr) r_tr = x_rad;
		if (corner_bl) r_bl = x_rad;
		if (corner_br) r_br = x_rad;
	}
	paintBoxRel(sx + r_tl,    sy          , dx - r_tl - r_tr, px,               col); // top horizontal
	paintBoxRel(sx + r_bl,    sy + dy - px, dx - r_bl - r_br, px,               col); // bottom horizontal
	paintBoxRel(sx          , sy + r_tl,    px,               dy - r_tl - r_bl, col); // left vertical
	paintBoxRel(sx + dx - px, sy + r_tr,    px,               dy - r_tr - r_br, col); // right vertical

	if (!radius || !type)
		return;

	radius = limitRadius(dx, dy, radius);
	if (radius < 1)		/* dx or dy = 0... */
		radius = 1;	/* avoid div by zero below */
	int line = 0;
	while (line < dy) {
		int ofs = 0, ofs_i = 0;
		// inner box
		if ((line >= px) && (line < (dy - px)))
			calcCorners(&ofs_i, NULL, NULL, dy - 2 * px, line - px, radius - px,
					corner_tl, corner_tr, corner_bl, corner_br);
		// outer box
		calcCorners(&ofs, NULL, NULL, dy, line, radius, corner_tl, corner_tr, corner_bl, corner_br);

		int _y     = sy + line;
		if (line < px || line >= (dy - px)) {
			// left
			if ((corner_tl && line < radius) || (corner_bl && line >= dy - radius))
				accel->paintLine(sx + ofs, _y, sx + ofs + radius, _y, col);
			// right
			if ((corner_tr && line < radius) || (corner_br && line >= dy - radius))
				accel->paintLine(sx + dx - radius, _y, sx + dx - ofs, _y, col);
		}
		else if (line < (dy - px)) {
			int _dx = (ofs_i - ofs) + px;
			// left
			if ((corner_tl && line < radius) || (corner_bl && line >= dy - radius))
				accel->paintLine(sx + ofs, _y, sx + ofs + _dx, _y, col);
			// right
			if ((corner_tr && line < radius) || (corner_br && line >= dy - radius))
				accel->paintLine(sx + dx - ofs_i - px, _y, sx + dx - ofs_i - px + _dx, _y, col);
		}
		if (line == radius && dy > 2 * radius)
			// line outside the rounded corners
			line = dy - radius;
		else
			line++;
	}
}

void CFrameBuffer::useBackground(bool ub)
{
	useBackgroundPaint = ub;
	if(!useBackgroundPaint) {
		delete[] background;
		background=0;
	}
}

bool CFrameBuffer::getuseBackground(void)
{
	return useBackgroundPaint;
}

void CFrameBuffer::saveBackgroundImage(void)
{
	if (backupBackground != NULL){
		delete[] backupBackground;
		backupBackground = NULL;
	}
	backupBackground = background;
	//useBackground(false); // <- necessary since no background is available
	useBackgroundPaint = false;
	background = NULL;
}

void CFrameBuffer::restoreBackgroundImage(void)
{
	fb_pixel_t * tmp = background;

	if (backupBackground != NULL)
	{
		background = backupBackground;
		backupBackground = NULL;
	}
	else
		useBackground(false); // <- necessary since no background is available

	if (tmp != NULL){
		delete[] tmp;
		tmp = NULL;
	}
}

void CFrameBuffer::paintBackgroundBoxRel(int x, int y, int dx, int dy)
{
	if (!getActive())
		return;

	if(!useBackgroundPaint)
	{
		/* paintBoxRel does its own checkFbArea() */
		paintBoxRel(x, y, dx, dy, backgroundColor);
	}
	else
	{
		checkFbArea(x, y, dx, dy, true);
		uint8_t * fbpos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
		fb_pixel_t * bkpos = background + x + BACKGROUNDIMAGEWIDTH * y;
		for(int count = 0;count < dy; count++)
		{
			memmove(fbpos, bkpos, dx * sizeof(fb_pixel_t));
			fbpos += stride;
			bkpos += BACKGROUNDIMAGEWIDTH;
		}
		checkFbArea(x, y, dx, dy, false);
		accel->mark(x, y, x + dx, y + dy);
	}
}

void CFrameBuffer::paintBackground()
{
	if (!getActive())
		return;

	if (useBackgroundPaint && (background != NULL))
	{
		checkFbArea(0, 0, xRes, yRes, true);
		/* this does not really work anyway... */
		for (int i = 0; i < 576; i++)
			memmove(((uint8_t *)getFrameBufferPointer()) + i * stride, (background + i * BACKGROUNDIMAGEWIDTH), BACKGROUNDIMAGEWIDTH * sizeof(fb_pixel_t));
		checkFbArea(0, 0, xRes, yRes, false);
		accel->mark(0, 0, xRes, yRes);
	}
	else
	{
		paintBoxRel(0, 0, xRes, yRes, backgroundColor);
	}
}

void CFrameBuffer::SaveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive() || memp == NULL)
		return;

	checkFbArea(x, y, dx, dy, true);
	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++) {
		fb_pixel_t * dest = (fb_pixel_t *)pos;
		for (int i = 0; i < dx; i++)
			*(bkpos++) = *(dest++);
		pos += stride;
	}
#if 0
	/* todo: check what the problem with this is, it should be better -- probably caching issue */
	uint8_t * fbpos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memmove(bkpos, fbpos, dx * sizeof(fb_pixel_t));
		fbpos += stride;
		bkpos += dx;
	}
#endif
	checkFbArea(x, y, dx, dy, false);
}

void CFrameBuffer::RestoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive() || memp == NULL)
		return;

	checkFbArea(x, y, dx, dy, true);
	uint8_t * fbpos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memmove(fbpos, bkpos, dx * sizeof(fb_pixel_t));
		fbpos += stride;
		bkpos += dx;
	}
	accel->mark(x, y, x + dx, y + dy);
	checkFbArea(x, y, dx, dy, false);
}

void CFrameBuffer::Clear()
{
	paintBackground();
	//memset(getFrameBufferPointer(), 0, stride * yRes);
}

bool CFrameBuffer::Lock()
{
	if(locked)
		return false;
	locked = true;
	return true;
}

void CFrameBuffer::Unlock()
{
	locked = false;
}

void * CFrameBuffer::int_convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp, bool alpha)
{
	unsigned long i;
	unsigned int *fbbuff;
	unsigned long count = x * y;

	fbbuff = (unsigned int *) cs_malloc_uncached(count * sizeof(unsigned int));
	if(fbbuff == NULL) {
		printf("convertRGB2FB%s: Error: cs_malloc_uncached\n", ((alpha) ? " (Alpha)" : ""));
		return NULL;
	}

	if (alpha) {
		for(i = 0; i < count ; i++)
			fbbuff[i] = ((rgbbuff[i*4+3] << 24) & 0xFF000000) | 
			            ((rgbbuff[i*4]   << 16) & 0x00FF0000) | 
		        	    ((rgbbuff[i*4+1] <<  8) & 0x0000FF00) | 
			            ((rgbbuff[i*4+2])       & 0x000000FF);
	} else {
		switch (m_transparent) {
			case CFrameBuffer::TM_BLACK:
				for(i = 0; i < count ; i++) {
					transp = 0;
					if(rgbbuff[i*3] || rgbbuff[i*3+1] || rgbbuff[i*3+2])
						transp = 0xFF;
					fbbuff[i] = (transp << 24) | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
				}
				break;
			case CFrameBuffer::TM_INI:
				for(i = 0; i < count ; i++)
					fbbuff[i] = (transp << 24) | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
				break;
			case CFrameBuffer::TM_NONE:
			default:
				for(i = 0; i < count ; i++)
					fbbuff[i] = 0xFF000000 | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
				break;
		}
	}
	return (void *) fbbuff;
}

void * CFrameBuffer::convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp)
{
	return int_convertRGB2FB(rgbbuff, x, y, transp, false);
}

void * CFrameBuffer::convertRGBA2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y)
{
	return int_convertRGB2FB(rgbbuff, x, y, 0, true);
}

void CFrameBuffer::blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp, uint32_t yp, bool transp)
{
	accel->blit2FB(fbbuff, width, height, xoff, yoff, xp, yp, transp);
}

void CFrameBuffer::blitBox2FB(const fb_pixel_t* boxBuf, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff)
{
	checkFbArea(xoff, yoff, width, height, true);

	uint32_t swidth = stride / sizeof(fb_pixel_t);
	fb_pixel_t *fbp = getFrameBufferPointer() + (swidth * yoff);
	fb_pixel_t* data = (fb_pixel_t*)boxBuf;

	uint32_t line = 0;
	while (line < height) {
		fb_pixel_t *pixpos = &data[line * width];
		for (uint32_t pos = xoff; pos < xoff + width; pos++) {
			//don't paint backgroundcolor (*pixpos = 0x00000000)
			if (*pixpos)
				*(fbp + pos) = *pixpos;
			pixpos++;
		}
		fbp += swidth;
		line++;
	}
	accel->mark(xoff, yoff, xoff + width, yoff + height);

	checkFbArea(xoff, yoff, width, height, false);
}

void CFrameBuffer::displayRGB(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb, int transp)
{
        void *fbbuff = NULL;

        if(rgbbuff == NULL)
                return;

        /* correct panning */
        if(x_pan > x_size - (int)xRes) x_pan = 0;
        if(y_pan > y_size - (int)yRes) y_pan = 0;

        /* correct offset */
        if(x_offs + x_size > (int)xRes) x_offs = 0;
        if(y_offs + y_size > (int)yRes) y_offs = 0;

        /* blit buffer 2 fb */
        fbbuff = convertRGB2FB(rgbbuff, x_size, y_size, transp);
        if(fbbuff==NULL)
                return;

        /* ClearFB if image is smaller */
        /* if(x_size < (int)xRes || y_size < (int)yRes) */
        if(clearfb)
                CFrameBuffer::getInstance()->Clear();

        blit2FB(fbbuff, x_size, y_size, x_offs, y_offs, x_pan, y_pan);
        cs_free_uncached(fbbuff);
}

void CFrameBuffer::setFbArea(int element, int _x, int _y, int _dx, int _dy)
{
	if (_x == 0 && _y == 0 && _dx == 0 && _dy == 0) {
		// delete area
		for (fbarea_iterator_t it = v_fbarea.begin(); it != v_fbarea.end(); ++it) {
			if (it->element == element) {
				v_fbarea.erase(it);
				break;
			}
		}
		if (v_fbarea.empty()) {
			fbAreaActiv = false;
		}
	}
	else {
		// change area
		bool found = false;
		for (unsigned int i = 0; i < v_fbarea.size(); i++) {
			if (v_fbarea[i].element == element) {
				v_fbarea[i].x = _x;
				v_fbarea[i].y = _y;
				v_fbarea[i].dx = _dx;
				v_fbarea[i].dy = _dy;
				found = true;
				break;
			}
		}
		// set new area
		if (!found) {
			fb_area_t area;
			area.x = _x;
			area.y = _y;
			area.dx = _dx;
			area.dy = _dy;
			area.element = element;
			v_fbarea.push_back(area);
		}
		fbAreaActiv = true;
	}
}

int CFrameBuffer::checkFbAreaElement(int _x, int _y, int _dx, int _dy, fb_area_t *area)
{
	if (fb_no_check)
		return FB_PAINTAREA_MATCH_NO;

	if (_y > area->y + area->dy)
		return FB_PAINTAREA_MATCH_NO;
	if (_x + _dx < area->x)
		return FB_PAINTAREA_MATCH_NO;
	if (_x > area->x + area->dx)
		return FB_PAINTAREA_MATCH_NO;
	if (_y + _dy < area->y)
		return FB_PAINTAREA_MATCH_NO;
	return FB_PAINTAREA_MATCH_OK;
}

bool CFrameBuffer::_checkFbArea(int _x, int _y, int _dx, int _dy, bool prev)
{
	if (v_fbarea.empty())
		return true;

	for (unsigned int i = 0; i < v_fbarea.size(); i++) {
		int ret = checkFbAreaElement(_x, _y, _dx, _dy, &v_fbarea[i]);
		if (ret == FB_PAINTAREA_MATCH_OK) {
			switch (v_fbarea[i].element) {
				case FB_PAINTAREA_MUTEICON1:
					if (!do_paint_mute_icon)
						break;
					fb_no_check = true;
					if (prev)
						CAudioMute::getInstance()->hide();
					else
						CAudioMute::getInstance()->paint();
					fb_no_check = false;
					break;
				default:
					break;
			}
		}
	}

	return true;
}

/* TODO: can we get rid of that? */
void CFrameBuffer::mark(int x, int y, int dx, int dy)
{
	accel->mark(x, y, dx, dy);
};

/* argh. really not the right thing to do here... */
extern cVideo *videoDecoder;
void CFrameBuffer::showFrame(const std::string & filename)
{
	std::string picture = ICONSDIR_VAR + filename;
	if (access(picture.c_str(), F_OK))
		picture = iconBasePath + filename;
	if (filename.find("/", 0) != std::string::npos)
		picture = filename;

	videoDecoder->ShowPicture(picture.c_str());
}

void CFrameBuffer::stopFrame()
{
	videoDecoder->StopPicture();
}
