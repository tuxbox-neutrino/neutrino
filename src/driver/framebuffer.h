/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

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


#ifndef __framebuffer__
#define __framebuffer__
#include <config.h>

#include <linux/fb.h>
#include <linux/vt.h>

#include <stdint.h>
#include <string>

#define fb_pixel_t uint32_t

typedef struct fb_var_screeninfo t_fb_var_screeninfo;

#if 1
#define CORNER_TOP_LEFT		0x1
#define CORNER_TOP_RIGHT	0x2
#define CORNER_TOP			0x3
#define CORNER_BOTTOM_RIGHT	0x4
#define CORNER_RIGHT		0x6
#define CORNER_BOTTOM_LEFT	0x8
#define CORNER_LEFT			0x9
#define CORNER_BOTTOM		0xC
#define CORNER_ALL		0xF
#define SHADOW_OFFSET		6
#else
#define CORNER_TOP	0x1
#define CORNER_BOTTOM	0x2
#define CORNER_BOTH	0x3
#endif

/** Ausfuehrung als Singleton */
class CFrameBuffer
{
	private:

		CFrameBuffer();

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

		#ifdef USE_NEVIS_GXA
		int		  devmem_fd;		/* to access the GXA register we use /dev/mem */
		unsigned int	  smem_start;		/* as aquired from the fbdev, the framebuffers physical start address */
		volatile uint8_t *gxa_base;		/* base address for the GXA's register access */
		#endif /* USE_NEVIS_GXA */
	public:
#ifndef FB_USE_PALETTE
		fb_pixel_t realcolor[256];
#endif

		~CFrameBuffer();

		static CFrameBuffer* getInstance();

		int getIconWidth(const char * const filename);	// infos about icon dimensions
		int getIconHeight(const char * const filename);

		void init(const char * const fbDevice = "/dev/fb/0");
		int setMode(unsigned int xRes, unsigned int yRes, unsigned int bpp);


		int getFileHandle() const; //only used for plugins (games) !!
		t_fb_var_screeninfo *getScreenInfo();

		fb_pixel_t * getFrameBufferPointer() const; // pointer to framebuffer
		unsigned int getStride() const;             // size of a single line in the framebuffer (in bytes)
		unsigned int getScreenWidth(bool real = false);
		unsigned int getScreenHeight(bool real = false); 
		unsigned int getScreenX();
		unsigned int getScreenY();
		
		bool getActive() const;                     // is framebuffer active?
		void setActive(bool enable);                     // is framebuffer active?

		void setTransparency( int tr = 0 );
		void setBlendLevel(int blev1, int blev2);

		//Palette stuff
		void setAlphaFade(int in, int num, int tr);
		void paletteGenFade(int in, __u32 rgb1, __u32 rgb2, int num, int tr=0);
		void paletteSetColor(int i, __u32 rgb, int tr);
		void paletteSet(struct fb_cmap *map = NULL);

		//paint functions
		inline void paintPixel(fb_pixel_t * const dest, const uint8_t color) const
			{
#ifdef FB_USE_PALETTE
				*dest = color;
#else
				*dest = realcolor[color];
#endif
			};
		void paintPixel(int x, int y, const fb_pixel_t col);

		void paintBoxRel(const int x, const int y, const int dx, const int dy, const fb_pixel_t col, int radius = 0, int type = CORNER_ALL);
		inline void paintBox(int xa, int ya, int xb, int yb, const fb_pixel_t col) { paintBoxRel(xa, ya, xb - xa, yb - ya, col); }
		inline void paintBox(int xa, int ya, int xb, int yb, const fb_pixel_t col, int radius, int type) { paintBoxRel(xa, ya, xb - xa, yb - ya, col, radius, type); }

		void paintLine(int xa, int ya, int xb, int yb, const fb_pixel_t col);

		void paintVLine(int x, int ya, int yb, const fb_pixel_t col);
		void paintVLineRel(int x, int y, int dy, const fb_pixel_t col);

		void paintHLine(int xa, int xb, int y, const fb_pixel_t col);
		void paintHLineRel(int x, int dx, int y, const fb_pixel_t col);


		void setIconBasePath(const std::string & iconPath);

		bool paintIcon (const char * const filename, const int x, const int y, const unsigned char offset = 1);
		bool paintIcon (const std::string & filename, const int x, const int y, const unsigned char offset = 1);
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

		void ClearFrameBuffer();
		void showFrame(const std::string & filename);
		bool loadBackgroundPic(const std::string & filename, bool show = true);
};


#endif
