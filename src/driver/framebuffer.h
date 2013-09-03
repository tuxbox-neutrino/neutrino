/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#include "framebuffer_ng.h"
#if 0

#ifndef __framebuffer__
#define __framebuffer__
#include <config.h>

#include <stdint.h>
#include <linux/fb.h>
#include <linux/vt.h>

#include <string>
#include <vector>
#include <map>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#define fb_pixel_t uint32_t

typedef struct fb_var_screeninfo t_fb_var_screeninfo;

#define CORNER_TOP_LEFT		0x1
#define CORNER_TOP_RIGHT	0x2
#define CORNER_TOP		0x3
#define CORNER_BOTTOM_RIGHT	0x4
#define CORNER_RIGHT		0x6
#define CORNER_BOTTOM_LEFT	0x8
#define CORNER_LEFT		0x9
#define CORNER_BOTTOM		0xC
#define CORNER_ALL		0xF

#define FADE_TIME 10000
#define FADE_STEP 5
#define FADE_RESET 0xFFFF

#define WINDOW_SIZE_MAX		100 // %
#define WINDOW_SIZE_MIN		80 // %
#define ConnectLineBox_Width	16 // px

/** Ausfuehrung als Singleton */
class CFrameBuffer
{
	private:

		CFrameBuffer();
		OpenThreads::Mutex mutex;

		struct rgbData
		{
			uint8_t r;
			uint8_t g;
			uint8_t b;
		} __attribute__ ((packed));

		struct rawHeader
		{
			uint8_t width_lo;
			uint8_t width_hi;
			uint8_t height_lo;
			uint8_t height_hi;
			uint8_t transp;
		} __attribute__ ((packed));

		struct rawIcon
		{
			uint16_t width;
			uint16_t height;
			uint8_t transp;
			fb_pixel_t * data;
		};

		std::string     iconBasePath;

		int             fd, tty;
		fb_pixel_t *    lfb;
		int		available;
		fb_pixel_t *    background;
		fb_pixel_t *    backupBackground;
		fb_pixel_t      backgroundColor;
		std::string     backgroundFilename;
		bool            useBackgroundPaint;
		unsigned int	xRes, yRes, stride, bpp;
		t_fb_var_screeninfo screeninfo, oldscreen;
		fb_cmap cmap;
		__u16 red[256], green[256], blue[256], trans[256];

		void paletteFade(int i, __u32 rgb1, __u32 rgb2, int level);

		int 	kd_mode;
		struct	vt_mode vt_mode;
		bool	active;
		static	void switch_signal (int);
		fb_fix_screeninfo fix;
#ifdef USE_NEVIS_GXA
		int		  devmem_fd;		/* to access the GXA register we use /dev/mem */
		unsigned int	  smem_start;		/* as aquired from the fbdev, the framebuffers physical start address */
		volatile uint8_t *gxa_base;		/* base address for the GXA's register access */
#endif /* USE_NEVIS_GXA */
		bool locked;
		std::map<std::string, rawIcon> icon_cache;
		int cache_size;

		int *q_circle;
		bool corner_tl, corner_tr, corner_bl, corner_br;

		void * int_convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp, bool alpha);
		int m_transparent_default, m_transparent;
		// Unlocked versions (no mutex)
		void paintHLineRelInternal(int x, int dx, int y, const fb_pixel_t col);
		void paintVLineRelInternal(int x, int y, int dy, const fb_pixel_t col);

		void paintShortHLineRelInternal(const int& x, const int& dx, const int& y, const fb_pixel_t& col);
		int  limitRadius(const int& dx, const int& dy, int& radius);
		void setCornerFlags(const int& type);
		void initQCircle();
		inline int calcCornersOffset(const int& dy, const int& line, const int& radius, const int& type) { int ofs = 0; calcCorners(&ofs, NULL, NULL, dy, line, radius, type); return ofs; }
		bool calcCorners(int *ofs, int *ofl, int *ofr, const int& dy, const int& line, const int& radius, const int& type);

	public:
		fb_pixel_t realcolor[256];

		~CFrameBuffer();

		static CFrameBuffer* getInstance();
#ifdef USE_NEVIS_GXA
		void setupGXA(void);
#endif

		void init(const char * const fbDevice = "/dev/fb/0");
		int setMode(unsigned int xRes, unsigned int yRes, unsigned int bpp);


		int getFileHandle() const; //only used for plugins (games) !!
		t_fb_var_screeninfo *getScreenInfo();

		fb_pixel_t * getFrameBufferPointer() const; // pointer to framebuffer
		unsigned int getStride() const;             // size of a single line in the framebuffer (in bytes)
		unsigned int getScreenWidth(bool real = false);
		unsigned int getScreenHeight(bool real = false); 
		unsigned int getScreenWidthRel(bool force_small = false);
		unsigned int getScreenHeightRel(bool force_small = false);
		unsigned int getScreenX();
		unsigned int getScreenY();
		
		bool getActive() const;                     // is framebuffer active?
		void setActive(bool enable);                     // is framebuffer active?

		void setTransparency( int tr = 0 );
		void setBlendMode(uint8_t mode = 1);
		void setBlendLevel(int level);

		//Palette stuff
		void setAlphaFade(int in, int num, int tr);
		void paletteGenFade(int in, __u32 rgb1, __u32 rgb2, int num, int tr=0);
		void paletteSetColor(int i, __u32 rgb, int tr);
		void paletteSet(struct fb_cmap *map = NULL);

		//paint functions
		inline void paintPixel(fb_pixel_t * const dest, const uint8_t color) const
			{
				*dest = realcolor[color];
			};
		void paintPixel(int x, int y, const fb_pixel_t col);

		void paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius = 0, int type = CORNER_ALL);
		inline void paintBox(int xa, int ya, int xb, int yb, const fb_pixel_t col) { paintBoxRel(xa, ya, xb - xa, yb - ya, col); }
		inline void paintBox(int xa, int ya, int xb, int yb, const fb_pixel_t col, int radius, int type) { paintBoxRel(xa, ya, xb - xa, yb - ya, col, radius, type); }

		void paintBoxFrame(const int x, const int y, const int dx, const int dy, const int px, const fb_pixel_t col, int radius = 0, int type = CORNER_ALL);
		void paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col);

		inline void paintVLine(int x, int ya, int yb, const fb_pixel_t col) { paintVLineRel(x, ya, yb - ya, col); }
		void paintVLineRel(int x, int y, int dy, const fb_pixel_t col);

		inline void paintHLine(int xa, int xb, int y, const fb_pixel_t col) { paintHLineRel(xa, xb - xa, y, col); }
		void paintHLineRel(int x, int dx, int y, const fb_pixel_t col);

		void setIconBasePath(const std::string & iconPath);

		void getIconSize(const char * const filename, int* width, int *height);
		/* h is the height of the target "window", if != 0 the icon gets centered in that window */
		bool paintIcon (const std::string & filename, const int x, const int y, 
				const int h = 0, const unsigned char offset = 1, bool paint = true, bool paintBg = false, const fb_pixel_t colBg = 0);
		bool paintIcon8(const std::string & filename, const int x, const int y, const unsigned char offset = 0);
		void loadPal   (const std::string & filename, const unsigned char offset = 0, const unsigned char endidx = 255);

		bool loadPicture2Mem        (const std::string & filename, fb_pixel_t * const memp);
		bool loadPicture2FrameBuffer(const std::string & filename);
		bool loadPictureToMem       (const std::string & filename, const uint16_t width, const uint16_t height, const uint16_t stride, fb_pixel_t * const memp);
		bool savePictureFromMem     (const std::string & filename, const fb_pixel_t * const memp);

		int getBackgroundColor() { return backgroundColor;}
		void setBackgroundColor(const fb_pixel_t color);
		bool loadBackground(const std::string & filename, const unsigned char col = 0);
		void useBackground(bool);
		bool getuseBackground(void);

		void saveBackgroundImage(void);  // <- implies useBackground(false);
		void restoreBackgroundImage(void);

		void paintBackgroundBoxRel(int x, int y, int dx, int dy);
		inline void paintBackgroundBox(int xa, int ya, int xb, int yb) { paintBackgroundBoxRel(xa, ya, xb - xa, yb - ya); }

		void paintBackground();

		void SaveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp);
		void RestoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp);

		void Clear();
		void showFrame(const std::string & filename);
		bool loadBackgroundPic(const std::string & filename, bool show = true);
		bool Lock(void);
		void Unlock(void);
		bool Locked(void) { return locked; };
#ifdef USE_NEVIS_GXA
		void add_gxa_sync_marker(void);
		void waitForIdle(const char* func=NULL);
#else
		inline void waitForIdle(const char*) {};
#endif
		void* convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp = 0xFF);
		void* convertRGBA2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y);
		void displayRGB(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb = true, int transp = 0xFF);
		void blit2FB(void *fbbuff, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff, uint32_t xp = 0, uint32_t yp = 0, bool transp = false);
		bool blitToPrimary(unsigned int * data, int dx, int dy, int sw, int sh);

		enum 
			{
				TM_EMPTY  = 0,
				TM_NONE   = 1,
				TM_BLACK  = 2,
				TM_INI    = 3
			};
		void SetTransparent(int t){ m_transparent = t; }
		void SetTransparentDefault(){ m_transparent = m_transparent_default; }

// ## AudioMute / Clock ######################################
	private:
		enum {
			FB_PAINTAREA_MATCH_NO,
			FB_PAINTAREA_MATCH_OK
		};

		typedef struct fb_area_t
		{
			int x;
			int y;
			int dx;
			int dy;
			int element;
		} fb_area_struct_t;

		bool fbAreaActiv;
		typedef std::vector<fb_area_t> v_fbarea_t;
		typedef v_fbarea_t::iterator fbarea_iterator_t;
		v_fbarea_t v_fbarea;
		bool fb_no_check;
		bool do_paint_mute_icon;

		bool _checkFbArea(int _x, int _y, int _dx, int _dy, bool prev);
		int checkFbAreaElement(int _x, int _y, int _dx, int _dy, fb_area_t *area);

	public:
		enum {
			FB_PAINTAREA_INFOCLOCK,
			FB_PAINTAREA_MUTEICON1,
			FB_PAINTAREA_MUTEICON2,

			FB_PAINTAREA_MAX
		};

		inline bool checkFbArea(int _x, int _y, int _dx, int _dy, bool prev) { return (fbAreaActiv && !fb_no_check) ? _checkFbArea(_x, _y, _dx, _dy, prev) : true; }
		void setFbArea(int element, int _x=0, int _y=0, int _dx=0, int _dy=0);
		void fbNoCheck(bool noCheck) { fb_no_check = noCheck; }
		void doPaintMuteIcon(bool mode) { do_paint_mute_icon = mode; }
		void blit(void) {}
};

#endif
#endif
