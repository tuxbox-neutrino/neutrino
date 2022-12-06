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

#ifndef __lcddisplay__
#define __lcddisplay__

#include <string>
#include <stdint.h> /* uint8_t */

#define LCD_DEVICE		"/dev/dbox/lcd0"

#define LCD_PIXEL_OFF		0x00
#define LCD_PIXEL_ON		0xff
#define LCD_PIXEL_INV		0x1000000

// ioctls

#define LCD_IOCTL_CLEAR		(26)

#define FP_IOCTL_LCD_DIMM	3

#ifndef LCD_IOCTL_ASC_MODE
#define LCDSET                  0x1000
#define LCD_IOCTL_ASC_MODE	(21|LCDSET)
#define	LCD_MODE_ASC		0
#define	LCD_MODE_BIN		1
#endif

#ifndef LCD_IOCTL_SRV
#define LCDSET                  0x1000
#define	LCD_IOCTL_SRV			(10|LCDSET)
#endif

typedef unsigned char *raw_display_t;

struct raw_lcd_element_header_t
{
	uint16_t width;
	uint16_t height;
	uint8_t bpp;
} __attribute__((packed));

struct raw_lcd_element_t
{
	raw_lcd_element_header_t header;
	int buffer_size;
	raw_display_t buffer;
};

class CLCDDisplay
{
	private:
		int           fd, paused;
		std::string   iconBasePath;
		bool          available;

		unsigned char inverted;
		bool flipped;
		int is_oled;	//1=oled, 2=lcd, 3=???
		int last_brightness;

		raw_display_t _buffer;
		int _stride;
		raw_display_t surface_data;
		int surface_stride;
		int surface_bpp, surface_bypp;
		int surface_buffer_size;
		int real_offset;
		int real_yres;

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
		void setIconBasePath(std::string bp)
		{
			iconBasePath = bp;
		};
		bool isAvailable();

		void update();

		void surface_fill_rect(int area_left, int area_top, int area_right, int area_bottom, int color);
		void draw_point(const int x, const int y, const int state);
		void draw_line(const int x1, const int y1, const int x2, const int y2, const int state);
		void draw_fill_rect(int left, int top, int right, int bottom, int state);
		void draw_rectangle(int left, int top, int right, int bottom, int linestate, int fillstate);
		void draw_polygon(int num_vertices, int *vertices, int state);

		bool paintIcon(std::string filename, int x, int y, bool invert);
		void clear_screen();
		void dump_screen(raw_display_t *screen);
		void load_screen_element(const raw_lcd_element_t *element, int left, int top);
		void load_screen(const raw_display_t *const screen);
		bool dump_png_element(const char *const filename, raw_lcd_element_t *element);
		bool dump_png(const char *const filename);
		bool load_png_element(const char *const filename, raw_lcd_element_t *element);
		bool load_png(const char *const filename);

		void setSize(int w, int h, int b);
		int setLCDContrast(int contrast);
		int setLCDBrightness(int brightness);
		void setInverted(unsigned char);
		void setFlipped(bool);
		bool isOled() const
		{
			return !!is_oled;
		}
		int raw_buffer_size;
		int xres, yres, bpp;
		int bypp;
};

#endif
