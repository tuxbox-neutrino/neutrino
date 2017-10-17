#ifndef __lcddisplay__
#define __lcddisplay__
/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
		baseroutines by Shadow_
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

#ifdef HAVE_SPARK_HARDWARE
#error src/lcddisplay/lcddisplay.h must not be included on SPARK
#endif
#ifdef HAVE_GENERIC_HARDWARE
// dummy
#define LCD_ROWS	8
#define LCD_COLS	120
#define LCD_PIXEL_OFF	0
#define LCD_PIXEL_ON	1
#define LCD_PIXEL_INV	2

#else
#ifndef HAVE_TRIPLEDRAGON
/* dreambox is actually compatible to dbox2 wrt. lcd */
#include <dbox/lcd-ks0713.h>
#define LCD_DEVICE	"/dev/dbox/lcd0"
#else
#include <tdpanel/lcdstuff.h>
#include <tddevices.h>
#define LCD_LINES	64
#define LCD_ROWS	(LCD_LINES / 8) // compatibility with stupid DBOX LCD driver
#define LCD_COLS	128
#define LCD_STRIDE	(LCD_COLS / 8)
#define LCD_BUFFER_SIZE	(LCD_LINES * LCD_STRIDE)
#define LCD_PIXEL_OFF	0
#define LCD_PIXEL_ON	1
#define LCD_PIXEL_INV	2
#define LCD_DEVICE	"/dev/" DEVICE_NAME_LCD
#define LCD_MODE_ASC	0
#define LCD_MODE_BIN	2
// ioctls
#define LCD_IOCTL_ASC_MODE	IOC_LCD_WRMODE
#define LCD_IOCTL_CLEAR		IOC_LCD_CLEAR
#endif
#endif

#include <string>

typedef unsigned char raw_display_t[LCD_ROWS*8][LCD_COLS];

class CLCDDisplay
{
 private:
	raw_display_t raw;
#ifdef HAVE_TRIPLEDRAGON
	unsigned char lcd[LCD_BUFFER_SIZE];
#else
	unsigned char lcd[LCD_ROWS][LCD_COLS];
#endif
	int           fd, paused;
	std::string   iconBasePath;
	bool          available;
	
 public:
	enum
		{
			PIXEL_ON  = LCD_PIXEL_ON,
			PIXEL_OFF = LCD_PIXEL_OFF,
			PIXEL_INV = LCD_PIXEL_INV
		};
	
		CLCDDisplay();
		~CLCDDisplay();

		void pause();
		void resume();

		void convert_data();
		void setIconBasePath(std::string bp){iconBasePath=bp;};
		bool isAvailable();

		void update();

		void draw_point(const int x, const int y, const int state);
		void draw_line(const int x1, const int y1, const int x2, const int y2, const int state);
		void draw_fill_rect (int left,int top,int right,int bottom,int state);
		void draw_rectangle (int left,int top, int right, int bottom, int linestate,int fillstate);
		void draw_polygon(int num_vertices, int *vertices, int state);

		bool paintIcon(std::string filename, int x, int y, bool invert);
		void dump_screen(raw_display_t *screen);
		void load_screen(const raw_display_t * const screen);
		bool load_png(const char * const filename);
};


#endif
