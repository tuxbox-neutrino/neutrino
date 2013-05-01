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

#include <config.h>
#ifdef HAVE_SPARK_HARDWARE
#define HAVE_GENERIC_HARDWARE 1
#endif
#include "lcddisplay.h"

#include <png.h>

#include <stdint.h> /* uint8_t */
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>


CLCDDisplay::CLCDDisplay()
{
	paused=0;
	available = false;
#ifndef HAVE_GENERIC_HARDWARE
	//open device
	if ((fd = open(LCD_DEVICE, O_RDWR)) < 0)
	{
		perror("LCD (" LCD_DEVICE ")");
		return;
	}

	//clear the display
	if ( ioctl(fd,LCD_IOCTL_CLEAR) < 0)
	{
		perror("clear failed");
		return;
	}
	
	//graphic (binary) mode 
#ifdef HAVE_TRIPLEDRAGON
	if (ioctl(fd, IOC_LCD_WRMODE, LCD_MODE_BIN) < 0)
#else
	int i=LCD_MODE_BIN;
	if ( ioctl(fd,LCD_IOCTL_ASC_MODE,&i) < 0 )
#endif
	{
		perror("graphic mode failed");
		return;
	}

	available = true;
#endif
	iconBasePath = "";
}

bool CLCDDisplay::isAvailable()
{
	return available;
}

CLCDDisplay::~CLCDDisplay()
{
	close(fd);
}

void CLCDDisplay::pause()
{
	paused = 1;
}

void CLCDDisplay::resume()
{
#ifndef HAVE_GENERIC_HARDWARE
	//clear the display
	if ( ioctl(fd,LCD_IOCTL_CLEAR) < 0)
	{
		perror("clear failed");
		return;
	}
	
	//graphic (binary) mode 
#ifdef HAVE_TRIPLEDRAGON
	if (ioctl(fd, IOC_LCD_WRMODE, LCD_MODE_BIN) < 0)
#else
	int i=LCD_MODE_BIN;
	if ( ioctl(fd,LCD_IOCTL_ASC_MODE,&i) < 0 )
#endif
	{
		perror("graphic mode failed");
		return;
	}
#endif
	paused = 0;
}

#ifdef HAVE_TRIPLEDRAGON
void CLCDDisplay::convert_data()
{
	int x, y, xx, xp, yp;
	unsigned char pix, bit;

	/* 128x64 (8bpp) membuffer -> 16*64 (1bpp) lcdbuffer */
	/* TODO: extend skins to 128x64 */

	/* the strange offset handling comes from a bug (probably) in the
	   TD LCD driver: the MSB (0x80) of the first byte (lcd[0]) written to
	   the device actually appears on the lower right, 8 pixels up, so we
	   must shift the whole buffer one pixel to the right. This is wrong for
	   the column 127 (rightmost), which is shifted up 8 lines.
	 */
	for (y = 0; y < LCD_LINES; y++) {
		for (x = 0; x < LCD_STRIDE; x++) {
			pix = 0;
			bit = 0x80;

			for (xx = x * 8; xx < x * 8 + 8; xx++, bit >>= 1) {
				xp = xx - 1;		/* shift the whole buffer one pixel to the right */
				yp = y;
				if (xp < 0) {		/* rightmost column (column 127) */
					xp += LCD_COLS;	/* wraparound */
					yp -= 8;	/* shift down 8 lines */
					if (yp < 0)	/* wraparound */
						yp += LCD_LINES;
				}
				if (raw[yp][xp] == 1)
					pix |= bit;
			}
/* I was chasing this one for quite some time, so check it for now */
#if 1
		if (y * LCD_STRIDE + x > LCD_BUFFER_SIZE)
			fprintf(stderr, "%s: y (%d) * LCD_STRIDE (%d) + x (%d) (== %d) > LCD_BUFFER_SIZE (%d)\n",
				__FUNCTION__, y, LCD_STRIDE, x, y*LCD_STRIDE+x, LCD_BUFFER_SIZE);
		else
#endif
			lcd[y * LCD_STRIDE + x] = pix;
		}
	}

#if 0
/* alternative solution, just ignore the rightmost column (which would be the
   MSB of the first byte */
	for (y=0; y<64; y++){
		for (x=0; x<(128/8); x++){
			int pix=0, start = 0;
			unsigned char bit = 0x80;
			int offset=(y*128)+x*8;
			if (x == 0) {	/* first column, skip MSB */
				start = 1;
				bit = 0x40;
			} else
				offset--;
			for (yy=start; yy<8; yy++, bit >>=1) {
				pix|=(_buffer[offset++]>=108)?bit:0;
			}
			raw[y*16+x]=pix;
		}
	}
#endif
}
#else
void CLCDDisplay::convert_data ()
{
#ifndef HAVE_GENERIC_HARDWARE
	unsigned int x, y, z;
	char tmp;

	for (x = 0; x < LCD_COLS; x++)
	{   
		for (y = 0; y < LCD_ROWS; y++)
		{
			tmp = 0;

			for (z = 0; z < 8; z++)
				if (raw[y * 8 + z][x] == 1)
					tmp |= (1 << z);

			lcd[y][x] = tmp;
		}
	}
#endif
}
#endif

void CLCDDisplay::update()
{
#ifndef HAVE_GENERIC_HARDWARE
	convert_data();
	if(paused || !available)
		return;
	else
		if ( write(fd, lcd, LCD_BUFFER_SIZE) < 0) {
			perror("lcdd: CLCDDisplay::update(): write()");
		}
#endif
}

void CLCDDisplay::draw_point(const int x, const int y, const int state)
{
	if ((x < 0) || (x >= LCD_COLS) || (y < 0) || (y >= (LCD_ROWS * 8)))
		return;

	if (state == LCD_PIXEL_INV)
		raw[y][x] ^= 1;
	else
		raw[y][x] = state;
}


/*
 * draw_line
 * 
 * args:
 * x1    StartCol
 * y1    StartRow
 * x2    EndCol
 * y1    EndRow
 * state LCD_PIXEL_OFF/LCD_PIXEL_ON/LCD_PIXEL_INV
 * 
 */
void CLCDDisplay::draw_line(const int x1, const int y1, const int x2, const int y2, const int state)  
{
	int dx = abs (x1 - x2);
	int dy = abs (y1 - y2);
	int x;
	int y;
	int End;
	int step;

	if ( dx > dy )
	{
		int	p = 2 * dy - dx;
		int	twoDy = 2 * dy;
		int	twoDyDx = 2 * (dy-dx);

		if ( x1 > x2 )
		{
			x = x2;
			y = y2;
			End = x1;
			step = y1 < y2 ? -1 : 1;
		}
		else
		{
			x = x1;
			y = y1;
			End = x2;
			step = y2 < y1 ? -1 : 1;
		}

		draw_point(x, y, state);

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
			draw_point(x, y, state);
		}
	}
	else
	{
		int	p = 2 * dx - dy;
		int	twoDx = 2 * dx;
		int	twoDxDy = 2 * (dx-dy);

		if ( y1 > y2 )
		{
			x = x2;
			y = y2;
			End = y1;
			step = x1 < x2 ? -1 : 1;
		}
		else
		{
			x = x1;
			y = y1;
			End = y2;
			step = x2 < x1 ? -1 : 1;
		}

		draw_point(x, y, state);

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
			draw_point(x, y, state);
		}
	}
}


void CLCDDisplay::draw_fill_rect (int left,int top,int right,int bottom,int state) {
	int x,y;
	for(x = left + 1;x < right;x++) {  
		for(y = top + 1;y < bottom;y++) {
			draw_point(x,y,state);
		}
	}
}


void CLCDDisplay::draw_rectangle (int left,int top, int right, int bottom, int linestate,int fillstate)
{
	// coordinate checking in draw_pixel (-> you can draw lines only
	// partly on screen)

	draw_line(left,top,right,top,linestate);
	draw_line(left,top,left,bottom,linestate);
	draw_line(right,top,right,bottom,linestate);
	draw_line(left,bottom,right,bottom,linestate);
	draw_fill_rect(left,top,right,bottom,fillstate);  
}  


void CLCDDisplay::draw_polygon(int num_vertices, int *vertices, int state) 
{

	// coordinate checking in draw_pixel (-> you can draw polygons only
	// partly on screen)

	int i;
	for(i=0;i<num_vertices-1;i++) {
		draw_line(vertices[(i<<1)+0],
			vertices[(i<<1)+1],
			vertices[(i<<1)+2],
			vertices[(i<<1)+3],
			state);
	}
   
	draw_line(vertices[0],
		vertices[1],
		vertices[(num_vertices<<1)-2],
		vertices[(num_vertices<<1)-1],
		state);
}


struct rawHeader
{
	uint8_t width_lo;
	uint8_t width_hi;
	uint8_t height_lo;
	uint8_t height_hi;
	uint8_t transp;
} __attribute__ ((packed));

bool CLCDDisplay::paintIcon(std::string filename, int x, int y, bool invert)
{
	struct rawHeader header;
	uint16_t         stride;
	uint16_t         height;
	unsigned char *  pixpos;

	int _fd;
	filename = iconBasePath + filename;

	_fd = open(filename.c_str(), O_RDONLY );
	
	if (_fd==-1)
	{
		printf("\nerror while loading icon: %s\n\n", filename.c_str() );
		return false;
	}

	read(_fd, &header, sizeof(struct rawHeader));

	stride = ((header.width_hi << 8) | header.width_lo) >> 1;
	height = (header.height_hi << 8) | header.height_lo;

	unsigned char pixbuf[200];
	while (height-- > 0)
	{
		read(fd, &pixbuf, stride);
		pixpos = (unsigned char*) &pixbuf;
		for (int count2 = 0; count2 < stride; count2++)
		{
			unsigned char compressed = *pixpos;

			draw_point(x + (count2 << 1)    , y, ((((compressed & 0xf0) >> 4) != header.transp) ^ invert) ? PIXEL_ON : PIXEL_OFF);
			draw_point(x + (count2 << 1) + 1, y, (( (compressed & 0x0f)       != header.transp) ^ invert) ? PIXEL_ON : PIXEL_OFF);

			pixpos++;
		}
		y++;
	}
	
	close(_fd);
	return true;
}

void CLCDDisplay::dump_screen(raw_display_t *screen) {
	memmove(screen, raw, sizeof(raw_display_t));
}

void CLCDDisplay::load_screen(const raw_display_t * const screen) {
	memmove(raw, screen, sizeof(raw_display_t));
}

bool CLCDDisplay::load_png(const char * const filename)
{
	png_structp  png_ptr;
	png_infop    info_ptr;
	unsigned int i;
	unsigned int pass;
	unsigned int number_passes;
	int          bit_depth;
	int          color_type;
	int          interlace_type;
	png_uint_32  width;
	png_uint_32  height;
	png_byte *   fbptr;
	FILE *       fh;
	bool         ret_value = false;

	if ((fh = fopen(filename, "rb")))
	{
		if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
		{
			if (!(info_ptr = png_create_info_struct(png_ptr)))
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
			else
			{
				if (!setjmp(png_jmpbuf(png_ptr)))
				{
					png_init_io(png_ptr,fh);
					
					png_read_info(png_ptr, info_ptr);
					png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
					
					if (
						(color_type == PNG_COLOR_TYPE_PALETTE) &&
						(bit_depth  == 1                     ) &&
						(width      <= LCD_COLS              ) &&
						(height     == (LCD_ROWS * 8))
						)
					{
						png_set_packing(png_ptr); /* expand to 1 byte blocks */
						
						number_passes = png_set_interlace_handling(png_ptr);
						png_read_update_info(png_ptr,info_ptr);
						
						if (width == png_get_rowbytes(png_ptr, info_ptr))
						{
							ret_value = true;
							
							for (pass = 0; pass < number_passes; pass++)
							{
								fbptr = (png_byte *)raw;
								for (i = 0; i < height; i++, fbptr += LCD_COLS)
								{
									png_read_row(png_ptr, fbptr, NULL);
									/* if the PNG is smaller, than the display width... */
									if (width < LCD_COLS)	/* clear the area right of the PNG */
										memset(fbptr + width, 0, LCD_COLS - width);
								}
							}
							png_read_end(png_ptr, info_ptr);
						}
					}
				}
				png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
			}
		}
		fclose(fh);
	}
	return ret_value;
}
