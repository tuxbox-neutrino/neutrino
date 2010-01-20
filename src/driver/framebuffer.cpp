/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
                      2003 thegoodguy

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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/framebuffer.h>

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory.h>
#include <math.h>

#include <linux/kd.h>

#include <stdint.h>

#include <gui/color.h>
#include <gui/pictureviewer.h>
#include <global.h>
//#include <cnxtfb.h>
#include <video_cs.h>
extern cVideo * videoDecoder;

extern CPictureViewer * g_PicViewer;
#define BACKGROUNDIMAGEWIDTH 720

//#undef USE_NEVIS_GXA //FIXME
/*******************************************************************************/
#ifdef USE_NEVIS_GXA

#ifdef GXA_FG_COLOR_REG
#undef GXA_FG_COLOR_REG
#endif
#ifdef GXA_BG_COLOR_REG
#undef GXA_BG_COLOR_REG
#endif
#ifdef GXA_LINE_CONTROL_REG
#undef GXA_LINE_CONTROL_REG
#endif
#ifdef GXA_DEPTH_REG
#undef GXA_DEPTH_REG
#endif
#ifdef GXA_CONTENT_ID_REG
#undef GXA_CONTENT_ID_REG
#endif

#define GXA_POINT(x, y)         (((y) & 0x0FFF) << 16) | ((x) & 0x0FFF)
#define GXA_SRC_BMP_SEL(x)      (x << 8)
#define GXA_DST_BMP_SEL(x)      (x << 5)
#define GXA_PARAM_COUNT(x)      (x << 2)

#define GXA_CMD_REG		0x001C
#define GXA_FG_COLOR_REG	0x0020
#define GXA_BG_COLOR_REG        0x0024
#define GXA_LINE_CONTROL_REG    0x0038
#define GXA_BMP2_TYPE_REG       0x0050
#define GXA_BMP2_ADDR_REG       0x0054
#define GXA_DEPTH_REG		0x00F4
#define GXA_CONTENT_ID_REG      0x0144

#define GXA_CMD_BLT             0x00010800
#define GXA_CMD_NOT_ALPHA       0x00011000
#define GXA_CMD_NOT_TEXT        0x00018000
#define GXA_CMD_QMARK		0x00001000

/*
static unsigned int _read_gxa(volatile unsigned char *base_addr, unsigned int offset)
{
    return *(volatile unsigned int *)(base_addr + offset);
}
*/

static unsigned int _mark = 0;

static void _write_gxa(volatile unsigned char *base_addr, unsigned int offset, unsigned int value)
{
    while( (*(volatile unsigned int *)(base_addr + GXA_DEPTH_REG)) & 0x40000000)
    {};
    *(volatile unsigned int *)(base_addr + offset) = value;
}

/* this adds a tagged marker into the GXA queue. Once this comes out
   of the other end of the queue, all commands before it are finished */
void CFrameBuffer::add_gxa_sync_marker(void)
{
	unsigned int cmd = GXA_CMD_QMARK | GXA_PARAM_COUNT(1);
	// TODO: locking?
	_mark++;
	_mark &= 0x0000001F; /* bit 0x20 crashes the kernel, if set */
	_write_gxa(gxa_base, cmd, _mark);
	//fprintf(stderr, "%s: wrote %02x\n", __FUNCTION__, _mark);
}

/* wait until the current marker comes out of the GXA command queue */
void CFrameBuffer::waitForIdle(void)
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
		fprintf(stderr, "CFrameBuffer::waitForIdle: count is big (%d)!\n", count);
}
#endif /* USE_NEVIS_GXA */

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
	fd  = 0;
	tty = 0;
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

void CFrameBuffer::init(const char * const fbDevice)
{
	fd = open(fbDevice, O_RDWR);
	if(!fd) fd = open(fbDevice, O_RDWR);

	if (fd<0) {
		perror(fbDevice);
		goto nolfb;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &screeninfo)<0) {
		perror("FBIOGET_VSCREENINFO");
		goto nolfb;
	}

	memcpy(&oldscreen, &screeninfo, sizeof(screeninfo));

	fb_fix_screeninfo fix;
	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix)<0) {
		perror("FBIOGET_FSCREENINFO");
		goto nolfb;
	}

	available=fix.smem_len;
	printf("%dk video mem\n", available/1024);
	lfb=(fb_pixel_t*)mmap(0, available, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);

	if (!lfb) {
		perror("mmap");
		goto nolfb;
	}

#ifdef USE_NEVIS_GXA
	/* Open 7dev/mem for HW-register access */
	devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (devmem_fd < 0) {
		perror("Unable to open /dev/mem");
		goto nolfb;
	}

	/* mmap the GXA's base address */
	gxa_base = (volatile unsigned char*) mmap(0, 0x00040000, PROT_READ | PROT_WRITE, MAP_SHARED, devmem_fd, 0xE0600000);
	if (gxa_base == (void*) -1){
		perror("Unable to mmap /dev/mem");
		goto nolfb;
	}

	/* tell the GXA where the framebuffer to draw on starts */
	smem_start = (unsigned int) fix.smem_start;
        _write_gxa(gxa_base, GXA_BMP2_TYPE_REG, (3 << 16) | screeninfo.xres);
	_write_gxa(gxa_base, GXA_BMP2_ADDR_REG, (unsigned int) fix.smem_start);
	_write_gxa(gxa_base, GXA_CONTENT_ID_REG, 0);
#endif

#if 0
	if ((tty=open("/dev/vc/0", O_RDWR))<0) {
		perror("open (tty)");
		goto nolfb;
	}

	struct sigaction act;

	memset(&act,0,sizeof(act));
	act.sa_handler  = switch_signal;
	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR1,&act,NULL);
	sigaction(SIGUSR2,&act,NULL);

	struct vt_mode mode;

	if (-1 == ioctl(tty,KDGETMODE, &kd_mode)) {
		perror("ioctl KDGETMODE");
		goto nolfb;
	}

	if (-1 == ioctl(tty,VT_GETMODE, &vt_mode)) {
      		perror("ioctl VT_GETMODE");
		goto nolfb;
	}

	if (-1 == ioctl(tty,VT_GETMODE, &mode)) {
      		perror("ioctl VT_GETMODE");
		goto nolfb;
	}

	mode.mode   = VT_PROCESS;
	mode.waitv  = 0;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR2;

	if (-1 == ioctl(tty,VT_SETMODE, &mode)) {
		perror("ioctl VT_SETMODE");
		goto nolfb;
	}

	if (-1 == ioctl(tty,KDSETMODE, KD_GRAPHICS)) {
		perror("ioctl KDSETMODE");
		goto nolfb;
	}
#endif

	return;

nolfb:
	printf("framebuffer not available.\n");
	lfb=0;
}


CFrameBuffer::~CFrameBuffer()
{
	if (background) {
		delete[] background;
		background = NULL;
	}

	if (backupBackground) {
		delete[] backupBackground;
		backupBackground = NULL;
	}

#if 0
#ifdef RETURN_FROM_GRAPHICS_MODE
	if (-1 == ioctl(tty,KDSETMODE, kd_mode))
		perror("ioctl KDSETMODE");
#endif

	if (-1 == ioctl(tty,VT_SETMODE, &vt_mode))
		perror("ioctl VT_SETMODE");

	if (available)
		ioctl(fd, FBIOPUT_VSCREENINFO, &oldscreen);
#endif
	if (lfb)
		munmap(lfb, available);

	if (virtual_fb){
		delete[] virtual_fb;
		virtual_fb = NULL;
	}
	close(fd);
	close(tty);
}

int CFrameBuffer::getFileHandle() const
{
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
		return lfb;
	else
		return (fb_pixel_t *) virtual_fb;
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
	if (!available&&!active)
		return -1;

#if 0
	screeninfo.xres_virtual=screeninfo.xres=nxRes;
	screeninfo.yres_virtual=screeninfo.yres=nyRes;
	screeninfo.height=0;
	screeninfo.width=0;
	screeninfo.xoffset=screeninfo.yoffset=0;
	screeninfo.bits_per_pixel=nbpp;

	if (ioctl(fd, FBIOPUT_VSCREENINFO, &screeninfo)<0) {
		perror("FBIOPUT_VSCREENINFO");
	}

	if(1) {
		printf("SetMode: %dbits, red %d:%d green %d:%d blue %d:%d transp %d:%d\n",
		screeninfo.bits_per_pixel, screeninfo.red.length, screeninfo.red.offset, screeninfo.green.length, screeninfo.green.offset, screeninfo.blue.length, screeninfo.blue.offset, screeninfo.transp.length, screeninfo.transp.offset);
	}
	if ((screeninfo.xres!=nxRes) && (screeninfo.yres!=nyRes) && (screeninfo.bits_per_pixel!=nbpp))
	{
		printf("SetMode failed: wanted: %dx%dx%d, got %dx%dx%d\n",
		       nxRes, nyRes, nbpp,
		       screeninfo.xres, screeninfo.yres, screeninfo.bits_per_pixel);
		return -1;
	}
#endif

	xRes = screeninfo.xres;
	yRes = screeninfo.yres;
	bpp  = screeninfo.bits_per_pixel;
	fb_fix_screeninfo fix;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix)<0) {
		perror("FBIOGET_FSCREENINFO");
		return -1;
	}

	stride = fix.line_length;
	printf("FB: %dx%dx%d line length %d. %s nevis GXA accelerator.\n", xRes, yRes, bpp, stride,
#ifdef USE_NEVIS_GXA
		"Using"
#else
		"Not using"
#endif
	);

	memset(getFrameBufferPointer(), 0, stride * yRes);
        if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
                printf("screen unblanking failed\n");
        }
	return 0;
}

void CFrameBuffer::setTransparency( int /*tr*/ )
{
}

void CFrameBuffer::setBlendLevel(int /*blev1*/, int /*blev2*/)
{
#if 0
	unsigned short value = blev1;
	if (ioctl(fd, FBIO_CHANGEOPACITY, &value) < 0)
		printf("FBIO_CHANGEOPACITY failed.\n");
#endif
}

void CFrameBuffer::setAlphaFade(int in, int num, int tr)
{
	for (int i=0; i<num; i++) {
		cmap.transp[in+i]=tr;
	}
}

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
}

#if 0
void CFrameBuffer::paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	if (!getActive())
		return;

	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	for (int count = 0; count < dy; count++) {
		fb_pixel_t * dest = (fb_pixel_t *)pos;
		for (int i = 0; i < dx; i++)
			*(dest++) = col;
		pos += stride;
	}
}
#endif

void CFrameBuffer::paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius, int type)
{
    /* draw a filled rectangle (with additional round corners) */

    if (!getActive())
        return;

    int corner_tl = (type & CORNER_TOP_LEFT)     ? 1 : 0;
    int corner_tr = (type & CORNER_TOP_RIGHT)    ? 1 : 0;
    int corner_bl = (type & CORNER_BOTTOM_LEFT)  ? 1 : 0;
    int corner_br = (type & CORNER_BOTTOM_RIGHT) ? 1 : 0;

#ifndef USE_NEVIS_GXA
    int swidth = stride / sizeof(fb_pixel_t);
    fb_pixel_t *fbp = getFrameBufferPointer() + (swidth * y);
#endif

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

    int line = 0;
#ifdef USE_NEVIS_GXA
    unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

    _write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int) col);		/* setup the drawing color */
    _write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404); 		/* X is major axis, skip last pixel */
#endif

    if ((type) && (radius))
    {
	#define MUL 32768	/* just an multiplicator for all math to reduce rounding errors */
	int ofs, scf, scl, ofl, ofr;

	/* limit the radius */
	if (radius > dx)
	    radius = dx;
	if (radius > dy)
	    radius = dy;
	if (radius > 540)
	    radius = 540;
	if (radius < 1)		/* dx or dy = 0... */
		radius = 1;	/* avoid div by zero below */

	scf = (540 * MUL) / radius;

	while (line < dy)
	{
	    ofl = ofr = 0;

	    if (line < radius && (type & CORNER_TOP)) /* one of the top corners */
	    {
		/* uper round corners */
		scl = scf * (radius - line) / MUL;
		if ((scf * (radius - line) % MUL) >= (MUL / 2))	/* round up */
		    scl++;
		ofs = radius - (q_circle[scl] * MUL / scf);
		// ofl = corner_tl * ofs; // might depend on the arch if multiply is faster or not
		ofl = corner_tl ? ofs : 0;
		ofr = corner_tr ? ofs : 0;
	    }
	    else if ((line >= dy - radius) && (type & CORNER_BOTTOM)) /* one of the bottom corners */
	    {
		/* lower round corners */
		scl = scf * (radius - (dy - (line + 1))) / MUL;
		if ((scf * (radius - (dy - (line + 1))) % MUL) >= (MUL / 2))	/* round up */
		    scl++;
		ofs = radius - (q_circle[scl] * MUL / scf);
		ofl = corner_bl ? ofs : 0;
		ofr = corner_br ? ofs : 0;
	    }
#ifdef USE_NEVIS_GXA
	    _write_gxa(gxa_base, cmd, GXA_POINT(x + dx - ofr, y + line));		/* endig point */
	    _write_gxa(gxa_base, cmd, GXA_POINT(x      + ofl, y + line));		/* start point */
#else
	    for (int pos = x + ofl; pos < x + dx - ofr; pos++)
	    {
		*(fbp + pos) = col;
	    }
	    fbp += swidth;
#endif
	    line++;
	}
    }
    else
    {
	while (line < dy)
	{
#ifdef USE_NEVIS_GXA
    	    _write_gxa(gxa_base, cmd, GXA_POINT(x + dx, y + line));		/* endig point */
    	    _write_gxa(gxa_base, cmd, GXA_POINT(x,      y + line));		/* start point */
#else
	    for (int pos = x; pos < x + dx; pos++)
	    {
		*(fbp + pos) = col;
	    }
	    fbp += swidth;
#endif
	    line++;
	}
    }
#ifdef USE_NEVIS_GXA
    /* the GXA seems to do asynchronous rendering, so we add a sync marker
       to which the fontrenderer code can synchronize */
    add_gxa_sync_marker();
#endif
}

void CFrameBuffer::paintVLine(int x, int ya, int yb, const fb_pixel_t col)
{
	if (!getActive())
		return;

#ifdef USE_NEVIS_GXA
    /* draw a single vertical line from point x/ya to x/yb */
    unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

    _write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int) col);	/* setup the drawing color */
    _write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404); 	/* X is major axis, skip last pixel */
    _write_gxa(gxa_base, cmd, GXA_POINT(x, ya + (yb - ya)));	/* end point */
    _write_gxa(gxa_base, cmd, GXA_POINT(x, ya));		/* start point */
#else /* USE_NEVIS_GXA */

	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * ya;

	int dy = yb-ya;
	for (int count = 0; count < dy; count++) {
		*(fb_pixel_t *)pos = col;
		pos += stride;
	}
#endif	/* USE_NEVIS_GXA */
}

void CFrameBuffer::paintVLineRel(int x, int y, int dy, const fb_pixel_t col)
{
	if (!getActive())
		return;

#ifdef USE_NEVIS_GXA
    /* draw a single vertical line from point x/y with hight dx */
    unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

    _write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int) col);	/* setup the drawing color */
    _write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404); 	/* X is major axis, skip last pixel */
    _write_gxa(gxa_base, cmd, GXA_POINT(x, y + dy));		/* end point */
    _write_gxa(gxa_base, cmd, GXA_POINT(x, y));			/* start point */
#else /* USE_NEVIS_GXA */
#if 0
	fb_fillrect fillrect;
	fillrect.dx = x;
	fillrect.dy = y;
	fillrect.width = 1;
	fillrect.height = dy;
	fillrect.color = col;
	fillrect.rop = ROP_COPY;
	ioctl(fd, FBIO_FILL_RECT, &fillrect);
	return;
#else
	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;

	for(int count=0;count<dy;count++) {
		*(fb_pixel_t *)pos = col;
		pos += stride;
	}
#endif
#endif /* USE_NEVIS_GXA */
}

void CFrameBuffer::paintHLine(int xa, int xb, int y, const fb_pixel_t col)
{
	if (!getActive())
		return;

#ifdef USE_NEVIS_GXA
	/* draw a single horizontal line from point xa/y to xb/y */
	unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

	_write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int) col);	/* setup the drawing color */
	_write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404); 	/* X is major axis, skip last pixel */
	_write_gxa(gxa_base, cmd, GXA_POINT(xa + (xb - xa), y));	/* end point */
	_write_gxa(gxa_base, cmd, GXA_POINT(xa, y));		/* start point */
#else /* USE_NEVIS_GXA */

	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + xa * sizeof(fb_pixel_t) + stride * y;

	int dx = xb -xa;
	fb_pixel_t * dest = (fb_pixel_t *)pos;
	for (int i = 0; i < dx; i++)
		*(dest++) = col;
#endif /* USE_NEVIS_GXA */
}

void CFrameBuffer::paintHLineRel(int x, int dx, int y, const fb_pixel_t col)
{
	if (!getActive())
		return;

#ifdef USE_NEVIS_GXA
	/* draw a single horizontal line from point x/y with width dx */
	unsigned int cmd = GXA_CMD_NOT_TEXT | GXA_SRC_BMP_SEL(2) | GXA_DST_BMP_SEL(2) | GXA_PARAM_COUNT(2) | GXA_CMD_NOT_ALPHA;

	_write_gxa(gxa_base, GXA_FG_COLOR_REG, (unsigned int) col);	/* setup the drawing color */
	_write_gxa(gxa_base, GXA_LINE_CONTROL_REG, 0x00000404); 	/* X is major axis, skip last pixel */
	_write_gxa(gxa_base, cmd, GXA_POINT(x + dx, y));		/* end point */
	_write_gxa(gxa_base, cmd, GXA_POINT(x, y));			/* start point */
#else /* USE_NEVIS_GXA */
	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;

	fb_pixel_t * dest = (fb_pixel_t *)pos;
	for (int i = 0; i < dx; i++)
		*(dest++) = col;
#endif /* USE_NEVIS_GXA */
}

void CFrameBuffer::setIconBasePath(const std::string & iconPath)
{
	iconBasePath = iconPath;
}

int CFrameBuffer::getIconHeight(const char * const filename)
{
	struct rawHeader header;
	uint16_t         height;
	int              icon_fd;

	char *ptr = rindex(filename, '.');
	if (ptr) {
		*ptr = 0;
		std::string newname = iconBasePath + std::string(filename) + ".gif";
		*ptr = '.';
		if (!access(newname.c_str(), F_OK))
			return g_PicViewer->getHeight(newname.c_str());
	}

	icon_fd = open(filename, O_RDONLY);

	if (icon_fd == -1)
	{
		printf("Framebuffer getIconHeight: error while loading icon: %s\n", filename);
		return 0;
	}
	else
	{
		read(icon_fd, &header, sizeof(struct rawHeader));
		height = (header.height_hi << 8) | header.height_lo;
	}

	close(icon_fd);
	return height;
}

int CFrameBuffer::getIconWidth(const char * const filename)
{
	struct rawHeader header;
	uint16_t         width;
	int              icon_fd;

	char *ptr = rindex(filename, '.');
	if (ptr) {
		*ptr = 0;
		std::string newname = iconBasePath + std::string(filename) + ".gif";
		*ptr = '.';
		if (!access(newname.c_str(), F_OK))
			return g_PicViewer->getWidth(newname.c_str());
	}

	icon_fd = open(filename, O_RDONLY);

	if (icon_fd == -1)
	{
		printf("Framebuffer getIconWidth: error while loading icon: %s\n", filename);
		width = 0;
	}
	else
	{
		read(icon_fd, &header, sizeof(struct rawHeader));
		width = (header.width_hi << 8) | header.width_lo;
	}

	close(icon_fd);
	return width;
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
	return true;
}

/* paint icon at position x/y,
   if height h is given, center vertically between y and y+h
   offset is a color offset (probably only useful with palette) */
bool CFrameBuffer::paintIcon(const std::string & filename, const int x, const int y,
			     const int h, const unsigned char offset)
{
	if (!getActive())
		return false;

	//printf("%s(file, %d, %d, %d)\n", __FUNCTION__, x, y, offset);
	int  yy = y;

	char * ptr = rindex(filename.c_str(), '.');
	if(ptr) {
		*ptr = 0;
		std::string newname = iconBasePath + filename.c_str() + ".gif";
		*ptr = '.';
		if(!access(newname.c_str(), F_OK))
		{
			if (h != 0)
				yy += (h - g_PicViewer->getHeight(newname.c_str())) / 2;
			return g_PicViewer->DisplayImage(newname, x, yy, 0, 0);
		}
	}
	struct rawHeader header;
	uint16_t         width, height;
	int              lfd;
#if 0 // no need if we have whole / as r/w
	std::string iconBasePath1 = "/var/share/icons/";
	lfd = open((iconBasePath1 + filename).c_str(), O_RDONLY);
	if (lfd == -1)
		lfd = open((iconBasePath + filename).c_str(), O_RDONLY);
#endif
	uint8_t * data;
	uint8_t transp;

	std::map<std::string, rawIcon>::iterator it;
	it = icon_cache.find((iconBasePath + filename).c_str());
	if(it == icon_cache.end()) {
		struct rawIcon tmpIcon;

		lfd = open((iconBasePath + filename).c_str(), O_RDONLY);

		if (lfd == -1) {
			printf("paintIcon: error while loading icon: %s%s\n", iconBasePath.c_str(), filename.c_str());
			return false;
		}
		read(lfd, &header, sizeof(struct rawHeader));

		tmpIcon.width = width  = (header.width_hi  << 8) | header.width_lo;
		tmpIcon.height = height = (header.height_hi << 8) | header.height_lo;
		tmpIcon.transp = transp = header.transp;
		tmpIcon.data = (uint8_t*) malloc(width*height);
		data = tmpIcon.data;

		unsigned char pixbuf[768];
		for (int count = 0; count < height; count ++ ) {
			read(lfd, &pixbuf[0], width >> 1 );
			unsigned char *pixpos = &pixbuf[0];
			for (int count2 = 0; count2 < width >> 1; count2 ++ ) {
				unsigned char compressed = *pixpos;
				unsigned char pix1 = (compressed & 0xf0) >> 4;
				unsigned char pix2 = (compressed & 0x0f);
				*data++ = pix1;
				*data++ = pix2;
				pixpos++;
			}
		}
		close(lfd);

		data = tmpIcon.data;

		icon_cache.insert(std::pair <std::string, rawIcon> (iconBasePath + filename, tmpIcon));
	} else {
		data = it->second.data;
		width = it->second.width;
		height = it->second.height;
		transp = it->second.transp;
		//printf("paintIcon: cached %s %d x %d\n", (iconBasePath + filename).c_str(), width, height);
	}
	if (h != 0)
		yy += (h - height) / 2;

	uint8_t * d = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * yy;
	fb_pixel_t * d2;

	for (int count = 0; count < height; count++ ) {
		unsigned char *pixpos = &data[count * width];
		d2 = (fb_pixel_t *) d;
		for (int count2 = 0; count2 < width; count2++ ) {
			unsigned char pix = *pixpos;
			if (pix != transp) {
				paintPixel(d2, pix + offset);
			}
			d2++;
			pixpos++;
		}
		d += stride;
	}

#if 0
	read(lfd, &header, sizeof(struct rawHeader));

	width  = (header.width_hi  << 8) | header.width_lo;
	height = (header.height_hi << 8) | header.height_lo;

	if (h != 0)
		yy += (h - height) / 2;

	unsigned char pixbuf[768];
	uint8_t * d = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * yy;
	fb_pixel_t * d2;
	for (int count=0; count<height; count ++ ) {
		read(lfd, &pixbuf[0], width >> 1 );
		unsigned char *pixpos = &pixbuf[0];
		d2 = (fb_pixel_t *) d;
		for (int count2=0; count2<width >> 1; count2 ++ ) {
			unsigned char compressed = *pixpos;
			unsigned char pix1 = (compressed & 0xf0) >> 4;
			unsigned char pix2 = (compressed & 0x0f);
			if (pix1 != header.transp) {
				paintPixel(d2, pix1 + offset);
			}
			d2++;
			if (pix2 != header.transp) {
				paintPixel(d2, pix2 + offset);
			}
			d2++;
			pixpos++;
		}
		d += stride;
	}

	close(lfd);
#endif
	return true;
}

bool CFrameBuffer::paintIcon(const char * const filename, const int x, const int y,
			     const int h, const unsigned char offset)
{
//printf("%s(%s, %d, %d, %d)\n", __FUNCTION__, filename, x, y, offset);
	return paintIcon(std::string(filename), x, y, h, offset);
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

void CFrameBuffer::paintPixel(const int x, const int y, const fb_pixel_t col)
{
	if (!getActive())
		return;

	#ifdef USE_NEVIS_GXA
	paintHLineRel(x, 1, y, col);
	#else
	fb_pixel_t * pos = getFrameBufferPointer();
	pos += (stride / sizeof(fb_pixel_t)) * y;
	pos += x;

	*pos = col;
	#endif
}

void CFrameBuffer::paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col)
{
	if (!getActive())
		return;

//printf("%s(%d, %d, %d, %d, %.8X)\n", __FUNCTION__, xa, ya, xb, yb, col);

	int dx = abs (xa - xb);
	int dy = abs (ya - yb);
	int x;
	int y;
	int End;
	int step;

	if ( dx > dy )
	{
		int	p = 2 * dy - dx;
		int	twoDy = 2 * dy;
		int	twoDyDx = 2 * (dy-dx);

		if ( xa > xb )
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

		paintPixel (x, y, col);

		while( x < End )
		{
			x++;
			if ( p < 0 )
				p += twoDy;
			else
			{
				y += step;
				p += twoDyDx;
			}
			paintPixel (x, y, col);
		}
	}
	else
	{
		int	p = 2 * dx - dy;
		int	twoDx = 2 * dx;
		int	twoDxDy = 2 * (dx-dy);

		if ( ya > yb )
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

		paintPixel (x, y, col);

		while( y < End )
		{
			y++;
			if ( p < 0 )
				p += twoDx;
			else
			{
				x += step;
				p += twoDxDy;
			}
			paintPixel (x, y, col);
		}
	}
}

void CFrameBuffer::setBackgroundColor(const fb_pixel_t color)
{
	backgroundColor = color;
}

bool CFrameBuffer::loadPictureToMem(const std::string & filename, const uint16_t width, const uint16_t height, const uint16_t pstride, fb_pixel_t * memp)
{
	struct rawHeader header;
	int              lfd;

//printf("%s(%d, %d, memp)\n", __FUNCTION__, width, height);

	lfd = open((iconBasePath + filename).c_str(), O_RDONLY );

	if (lfd == -1)
	{
		printf("error while loading icon: %s%s\n", iconBasePath.c_str(), filename.c_str());
		return false;
	}

	read(lfd, &header, sizeof(struct rawHeader));

	if ((width  != ((header.width_hi  << 8) | header.width_lo)) ||
	    (height != ((header.height_hi << 8) | header.height_lo)))
	{
		printf("error while loading icon: %s - invalid resolution = %hux%hu\n", filename.c_str(), width, height);
		return false;
	}

	if ((pstride == 0) || (pstride == width * sizeof(fb_pixel_t)))
		read(lfd, memp, height * width * sizeof(fb_pixel_t));
	else
		for (int i = 0; i < height; i++)
			read(lfd, ((uint8_t *)memp) + i * pstride, width * sizeof(fb_pixel_t));

	close(lfd);
	return true;
}

bool CFrameBuffer::loadPicture2Mem(const std::string & filename, fb_pixel_t * memp)
{
	return loadPictureToMem(filename, BACKGROUNDIMAGEWIDTH, 576, 0, memp);
}

bool CFrameBuffer::loadPicture2FrameBuffer(const std::string & filename)
{
	if (!getActive())
		return false;

	return loadPictureToMem(filename, BACKGROUNDIMAGEWIDTH, 576, getStride(), getFrameBufferPointer());
}

bool CFrameBuffer::savePictureFromMem(const std::string & filename, const fb_pixel_t * const memp)
{
	struct rawHeader header;
	uint16_t         width, height;
	int              lfd;

	width = BACKGROUNDIMAGEWIDTH;
	height = 576;

	header.width_lo  = width  &  0xFF;
	header.width_hi  = width  >>    8;
	header.height_lo = height &  0xFF;
	header.height_hi = height >>    8;
	header.transp    =              0;

	lfd = open((iconBasePath + filename).c_str(), O_WRONLY | O_CREAT);

	if (lfd==-1)
	{
		printf("error while saving icon: %s%s", iconBasePath.c_str(), filename.c_str() );
		return false;
	}

	write(lfd, &header, sizeof(struct rawHeader));

	write(lfd, memp, width * height * sizeof(fb_pixel_t));

	close(lfd);
	return true;
}

bool CFrameBuffer::loadBackground(const std::string & filename, const unsigned char offset)
{
	if ((backgroundFilename == filename) && (background))
		return true;

	if (background)
		delete[] background;

	background = new fb_pixel_t[BACKGROUNDIMAGEWIDTH * 576];

	if (!loadPictureToMem(filename, BACKGROUNDIMAGEWIDTH, 576, 0, background))
	{
		delete[] background;
		background=0;
		return false;
	}

	if (offset != 0)//pic-offset
	{
		fb_pixel_t * bpos = background;
		int pos = BACKGROUNDIMAGEWIDTH * 576;
		while (pos > 0)
		{
			*bpos += offset;
			bpos++;
			pos--;
		}
	}

	fb_pixel_t * dest = background + BACKGROUNDIMAGEWIDTH * 576;
	uint8_t    * src  = ((uint8_t * )background)+ BACKGROUNDIMAGEWIDTH * 576;
	for (int i = 576 - 1; i >= 0; i--)
		for (int j = BACKGROUNDIMAGEWIDTH - 1; j >= 0; j--)
		{
			dest--;
			src--;
			paintPixel(dest, *src);
		}
	backgroundFilename = filename;

	return true;
}

bool CFrameBuffer::loadBackgroundPic(const std::string & filename, bool show)
{
	if ((backgroundFilename == filename) && (background))
		return true;

//printf("loadBackgroundPic: %s\n", filename.c_str());
	if (background){
		delete[] background;
		background = NULL;
	}
	background = g_PicViewer->getImage(iconBasePath + filename, BACKGROUNDIMAGEWIDTH, 576);

	if (background == NULL) {
		background=0;
		return false;
	}

	backgroundFilename = filename;
	if(show) {
		useBackgroundPaint = true;
		paintBackground();
	}
	return true;
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
		paintBoxRel(x, y, dx, dy, backgroundColor);
	}
	else
	{
		uint8_t * fbpos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
		fb_pixel_t * bkpos = background + x + BACKGROUNDIMAGEWIDTH * y;
		for(int count = 0;count < dy; count++)
		{
			memcpy(fbpos, bkpos, dx * sizeof(fb_pixel_t));
			fbpos += stride;
			bkpos += BACKGROUNDIMAGEWIDTH;
		}
	}
}

void CFrameBuffer::paintBackground()
{
	if (!getActive())
		return;

	if (useBackgroundPaint && (background != NULL))
	{
		for (int i = 0; i < 576; i++)
			memcpy(((uint8_t *)getFrameBufferPointer()) + i * stride, (background + i * BACKGROUNDIMAGEWIDTH), BACKGROUNDIMAGEWIDTH * sizeof(fb_pixel_t));
	}
	else
	{
		paintBoxRel(0, 0, xRes, yRes, backgroundColor);
	}
}

void CFrameBuffer::SaveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive())
		return;

	uint8_t * pos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++) {
		fb_pixel_t * dest = (fb_pixel_t *)pos;
		for (int i = 0; i < dx; i++)
			//*(dest++) = col;
			*(bkpos++) = *(dest++);
		pos += stride;
	}
#if 0 //FIXME test to flush cache
        if (ioctl(fd, 1, FB_BLANK_UNBLANK) < 0);
#endif
	//RestoreScreen(x, y, dx, dy, memp); //FIXME
#if 0
	uint8_t * fbpos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memcpy(bkpos, fbpos, dx * sizeof(fb_pixel_t));
		fbpos += stride;
		bkpos += dx;
	}
#endif

}

void CFrameBuffer::RestoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	if (!getActive())
		return;

	uint8_t * fbpos = ((uint8_t *)getFrameBufferPointer()) + x * sizeof(fb_pixel_t) + stride * y;
	fb_pixel_t * bkpos = memp;
	for (int count = 0; count < dy; count++)
	{
		memcpy(fbpos, bkpos, dx * sizeof(fb_pixel_t));
		fbpos += stride;
		bkpos += dx;
	}
}

void CFrameBuffer::switch_signal (int signal)
{
	CFrameBuffer * thiz = CFrameBuffer::getInstance();
	if (signal == SIGUSR1) {
		if (virtual_fb != NULL)
			delete[] virtual_fb;
		virtual_fb = new uint8_t[thiz->stride * thiz->yRes];
		thiz->active = false;
		if (virtual_fb != NULL)
			memcpy(virtual_fb, thiz->lfb, thiz->stride * thiz->yRes);
		ioctl(thiz->tty, VT_RELDISP, 1);
		printf ("release display\n");
	}
	else if (signal == SIGUSR2) {
		ioctl(thiz->tty, VT_RELDISP, VT_ACKACQ);
		thiz->active = true;
		printf ("acquire display\n");
		thiz->paletteSet(NULL);
		if (virtual_fb != NULL)
			memcpy(thiz->lfb, virtual_fb, thiz->stride * thiz->yRes);
		else
			memset(thiz->lfb, 0, thiz->stride * thiz->yRes);
	}
}

void CFrameBuffer::ClearFrameBuffer()
{
	int tr;

	tr = 0xFF;
	//paletteSetColor(0, 0xFFFFFF, 0xFF);
	//Windows Colors
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
	useBackground(false);

	paintBackground();

	//background
	paletteSetColor(COL_BACKGROUND, 0x000000, 0xffff);

	paletteSet();
}

void CFrameBuffer::showFrame(const std::string & filename)
{
	std::string varpath = "/var/tuxbox/config/neutrino/icons/";
	if(!access((varpath + filename).c_str(), F_OK))
		videoDecoder->ShowPicture((varpath + filename).c_str());
	else
		videoDecoder->ShowPicture((iconBasePath + filename).c_str());
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
