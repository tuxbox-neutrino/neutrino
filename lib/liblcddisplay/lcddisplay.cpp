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
#include <byteswap.h>
#include <string.h>

#ifndef BYTE_ORDER
#error "no BYTE_ORDER defined!"
#endif

#ifndef max
#define max(a,b)(((a)<(b)) ? (b) : (a))
#endif

#ifndef min
#define min(a,b)(((a)<(b)) ? (a) : (b))
#endif


void CLCDDisplay::setSize(int w, int h, int b)
{
	xres = w;
	yres = h;
	bpp = 8;
	bypp = 1;
	surface_bpp = b;

	real_offset = 0;
	real_yres = yres;
	if (yres == 32)
		real_offset = 16;
	if (yres < 64)
		yres = 48;

	switch (surface_bpp)
	{
		case 8:
			surface_bypp = 1;
			break;
		case 15:
		case 16:
			surface_bypp = 2;
			break;
		case 24:		// never use 24bit mode
		case 32:
			surface_bypp = 4;
			break;
		default:
			surface_bypp = (bpp + 7) / 8;
	}

	surface_stride = xres * surface_bypp;
	surface_buffer_size = xres * yres * surface_bypp;
	surface_data = new unsigned char[surface_buffer_size];
	memset(surface_data, 0, surface_buffer_size);

	printf("[CLCDDisplay] %s surface buffer %p %d bytes, stride %d\n", __FUNCTION__, surface_data, surface_buffer_size, surface_stride);

	_stride = xres * bypp;
	raw_buffer_size = xres * yres * bypp;
	_buffer = new unsigned char[raw_buffer_size];
	memset(_buffer, 0, raw_buffer_size);

	printf("[CLCDDisplay] %s lcd buffer %p %d bytes, stride %d, type %d\n", __FUNCTION__, _buffer, raw_buffer_size, _stride, is_oled);
}

CLCDDisplay::CLCDDisplay()
{
	paused = 0;
	available = false;

	raw_buffer_size = 0;
	xres = 132;
	yres = 32;
	bpp = 8;

	flipped = false;
	inverted = 0;
	is_oled = 0;
	last_brightness = 0;

	//open device
	fd = open("/dev/dbox/oled0", O_RDWR);

	if (fd < 0)
	{
		xres = 128;
		if (!access("/proc/stb/lcd/oled_brightness", W_OK) || !access("/proc/stb/fp/oled_brightness", W_OK))
			is_oled = 2;
		fd = open(LCD_DEVICE, O_RDWR);
	}
	else
	{
		printf("found OLED display!\n");
		is_oled = 1;
	}

	if (fd < 0)
	{
		printf("couldn't open LCD - load lcd.ko!\n");
		return;
	}
	else
	{
		int i = LCD_MODE_BIN;
		ioctl(fd, LCD_IOCTL_ASC_MODE, &i);

		FILE *f = fopen("/proc/stb/lcd/xres", "r");
		if (f)
		{
			int tmp;
			if (fscanf(f, "%x", &tmp) == 1)
				xres = tmp;
			fclose(f);
			f = fopen("/proc/stb/lcd/yres", "r");
			if (f)
			{
				if (fscanf(f, "%x", &tmp) == 1)
					yres = tmp;
				fclose(f);
				f = fopen("/proc/stb/lcd/bpp", "r");
				if (f)
				{
					if (fscanf(f, "%x", &tmp) == 1)
						bpp = tmp;
					fclose(f);
				}
			}
			is_oled = 3;
		}
	}

	setSize(xres, yres, bpp);

	available = true;
	iconBasePath = "";
}

//e2
void CLCDDisplay::setInverted(unsigned char inv)
{
	inverted = inv;
	update();
}

void CLCDDisplay::setFlipped(bool onoff)
{
	flipped = onoff;
	update();
}

int CLCDDisplay::setLCDContrast(int contrast)
{
	return(0);
	
	int fp;

	fp = open("/dev/dbox/fp0", O_RDWR);

	if (fp < 0)
		fp = open("/dev/dbox/lcd0", O_RDWR);
	if (fp < 0)
	{
		printf("[LCD] can't open /dev/dbox/fp0(%m)\n");
		return (-1);
	}

	if (ioctl(fd, LCD_IOCTL_SRV, &contrast) < 0)
	{
		printf("[LCD] can't set lcd contrast(%m)\n");
	}
	close(fp);

	return (0);
}

int CLCDDisplay::setLCDBrightness(int brightness)
{
	printf("setLCDBrightness %d\n", brightness);

	FILE *f = fopen("/proc/stb/lcd/oled_brightness", "w");
	if (!f)
		f = fopen("/proc/stb/fp/oled_brightness", "w");
	if (f)
	{
		if (fprintf(f, "%d", brightness) == 0)
			printf("write /proc/stb/lcd/oled_brightness failed!! (%m)\n");
		fclose(f);
	}
	else
	{
		int fp;
		if ((fp = open("/dev/dbox/fp0", O_RDWR)) < 0)
		{
			printf("[LCD] can't open /dev/dbox/fp0\n");
			return (-1);
		}

		if (ioctl(fp, FP_IOCTL_LCD_DIMM, &brightness) < 0)
			printf("[LCD] can't set lcd brightness (%m)\n");
		close(fp);
	}

	if (brightness == 0)
	{
		memset(_buffer, inverted, raw_buffer_size);
		update();
	}

	last_brightness = brightness;

	return (0);
}

bool CLCDDisplay::isAvailable()
{
	return available;
}

CLCDDisplay::~CLCDDisplay()
{
	delete [] _buffer;

	if (fd >= 0)
	{
		close(fd);
		fd = -1;
	}
}

void CLCDDisplay::pause()
{
	paused = 1;
}

void CLCDDisplay::resume()
{
	//clear the display
	if (ioctl(fd, LCD_IOCTL_CLEAR) < 0)
		printf("[lcddisplay] LCD_IOCTL_CLEAR failed (%m)\n");

	//graphic (binary) mode
	int i = LCD_MODE_BIN;
	if (ioctl(fd, LCD_IOCTL_ASC_MODE, &i) < 0)
		printf("[lcddisplay] LCD_IOCTL_ASC_MODE failed (%m)\n");
	//

	paused = 0;
}

void CLCDDisplay::convert_data()
{
#ifndef PLATFORM_GENERIC
	unsigned int x, y, z;
	char tmp;

#if 0
	unsigned int height = yres;
	unsigned int width = xres;

	for (x = 0; x < width; x++)
	{
		for (y = 0; y < (height / 8); y++)
		{
			tmp = 0;

			for (z = 0; z < 8; z++)
				if (_buffer[(y * 8 + z) * width + x] == LCD_PIXEL_ON)
					tmp |= (1 << z);

			lcd[y][x] = tmp;
		}
	}
#endif
#endif
}

void CLCDDisplay::update()
{
#ifndef PLATFORM_GENERIC
	if ((fd >= 0) && (last_brightness > 0))
	{
		for (unsigned int y = 0; y < yres; y++)
		{
			for (unsigned int x = 0; x < xres; x++)
			{
				surface_fill_rect(x, y, x + 1, y + 1, _buffer[y * xres + x]);
			}
		}

		if (is_oled == 0 || is_oled == 2)
		{
			unsigned int height = yres;
			unsigned int width = xres;


			unsigned char raw[132 * 8];
			int x, y, yy;

			memset(raw, 0x00, 132 * 8);

			for (y = 0; y < 8; y++)
			{
				// display has only 128 but buffer must be 132
				for (x = 0; x < 128; x++)
				{
					int pix = 0;
					for (yy = 0; yy < 8; yy++)
					{
						pix |= (surface_data[(y * 8 + yy) * width + x] >= 108) << yy;
					}

					if (flipped)
					{
						/* 8 pixels per byte, swap bits */
#define BIT_SWAP(a) ((((a << 7)&0x80) + ((a << 5)&0x40) + ((a << 3)&0x20) + ((a << 1)&0x10) + ((a >> 1)&0x08) + ((a >> 3)&0x04) + ((a >> 5)&0x02) + ((a >> 7)&0x01))&0xff)
						raw[(7 - y) * 132 + (132 - 1 - x - 2)] = BIT_SWAP(pix ^ inverted);
					}
					else
					{
						raw[y * 132 + x + 2] = pix ^ inverted;
					}
				}
			}

			write(fd, raw, 132 * 8);

		}
		else if (is_oled == 3)
		{
			/* for now, only support flipping / inverting for 8bpp displays */
			if ((flipped || inverted) && surface_stride == xres)
			{
				unsigned int height = yres;
				unsigned int width = xres;
				unsigned char raw[surface_stride * height];

				for (unsigned int y = 0; y < height; y++)
				{
					for (unsigned int x = 0; x < width; x++)
					{
						if (flipped)
						{
							/* 8bpp, no bit swapping */
							raw[(height - 1 - y) * width + (width - 1 - x)] = surface_data[y * width + x] ^ inverted;
						}
						else
						{
							raw[y * width + x] = surface_data[y * width + x] ^ inverted;
						}
					}
				}
				write(fd, raw, surface_stride * height);
			}
			else
			{
				//write(fd, surface_data, surface_stride * yres);
				//
#ifdef PLATFORM_GIGABLUE
				unsigned char gb_buffer[surface_stride * yres];
				for (int offset = 0; offset < surface_stride * yres; offset += 2)
				{
					gb_buffer[offset] = (surface_data[offset] & 0x1F) | ((surface_data[offset + 1] << 3) & 0xE0);
					gb_buffer[offset + 1] = ((surface_data[offset + 1] >> 5) & 0x03) | ((surface_data[offset] >> 3) & 0x1C) | ((_buffer[offset + 1] << 5) & 0x60);
				}
				write(fd, gb_buffer, surface_stride * yres);
#else
				write(fd, surface_data + surface_stride * real_offset, surface_stride * real_yres);
#endif
				//
			}
		}
		else /* is_oled == 1 */
		{
			unsigned char raw[64 * 64];
			int x, y;
			memset(raw, 0, 64 * 64);

			for (y = 0; y < 64; y++)
			{
				int pix = 0;
				for (x = 0; x < 128 / 2; x++)
				{
					pix = (surface_data[y * 132 + x * 2 + 2] & 0xF0) | (surface_data[y * 132 + x * 2 + 1 + 2] >> 4);
					if (inverted)
						pix = 0xFF - pix;
					if (flipped)
					{
						/* device seems to be 4bpp, swap nibbles */
						unsigned char byte;
						byte = (pix >> 4) & 0x0f;
						byte |= (pix << 4) & 0xf0;
						raw[(63 - y) * 64 + (63 - x)] = byte;
					}
					else
					{
						raw[y * 64 + x] = pix;
					}
				}
			}
			write(fd, raw, 64 * 64);
		}
	}
#endif
}

void CLCDDisplay::surface_fill_rect(int area_left, int area_top, int area_right, int area_bottom, int color)
{
	int area_width  = area_right - area_left;
	int area_height = area_bottom - area_top;

	if (surface_bpp == 8)
	{
		for (int y = area_top; y < area_bottom; y++)
			memset(((uint8_t *)surface_data) + y * surface_stride + area_left, color, area_width);
	}
	else if (surface_bpp == 16)
	{
		uint32_t icol;

#if 0
		if (surface_clut.data && color < surface_clut.colors)
			icol = (surface_clut.data[color].a << 24) | (surface_clut.data[color].r << 16) | (surface_clut.data[color].g << 8) | (surface_clut.data[color].b);
		else
#endif
			icol = 0x10101 * color;
#if BYTE_ORDER == LITTLE_ENDIAN
		uint16_t col = bswap_16(((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19);
#else
		uint16_t col = ((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19;
#endif
		for (int y = area_top; y < area_bottom; y++)
		{
			uint16_t *dst = (uint16_t *)(((uint8_t *)surface_data) + y * surface_stride + area_left * surface_bypp);
			int x = area_width;
			while (x--)
				*dst++ = col;
		}
	}
	else if (surface_bpp == 32)
	{
		uint32_t col;

#if 0
		if (surface_clut.data && color < surface_clut.colors)
			col = (surface_clut.data[color].a << 24) | (surface_clut.data[color].r << 16) | (surface_clut.data[color].g << 8) | (surface_clut.data[color].b);
		else
#endif
			col = 0x10101 * color;

		col ^= 0xFF000000;

#if 0
		if (surface_data_phys && gAccel::getInstance())
			if (!gAccel::getInstance()->fill(surface,  area, col))
				continue;
#endif

		for (int y = area_top; y < area_bottom; y++)
		{
			uint32_t *dst = (uint32_t *)(((uint8_t *)surface_data) + y * surface_stride + area_left * surface_bypp);
			int x = area_width;
			while (x--)
				*dst++ = col;
		}
	}
}

void CLCDDisplay::draw_point(const int x, const int y, const int state)
{
	if ((x < 0) || (x >= xres) || (y < 0) || (y >= yres))
		return;

	if (state == LCD_PIXEL_INV)
		_buffer[y * xres + x] ^= 1;
	else
		_buffer[y * xres + x] = state;
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
	int dx = abs(x1 - x2);
	int dy = abs(y1 - y2);
	int x;
	int y;
	int End;
	int step;

	if (dx > dy)
	{
		int	p = 2 * dy - dx;
		int	twoDy = 2 * dy;
		int	twoDyDx = 2 * (dy - dx);

		if (x1 > x2)
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

		while (x < End)
		{
			x++;
			if (p < 0)
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
		int	twoDxDy = 2 * (dx - dy);

		if (y1 > y2)
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

		while (y < End)
		{
			y++;
			if (p < 0)
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

void CLCDDisplay::draw_fill_rect(int left, int top, int right, int bottom, int state)
{
	int x, y;
	for (x = left + 1; x < right; x++)
	{
		for (y = top + 1; y < bottom; y++)
		{
			draw_point(x, y, state);
		}
	}
}

void CLCDDisplay::draw_rectangle(int left, int top, int right, int bottom, int linestate, int fillstate)
{
	// coordinate checking in draw_pixel (-> you can draw lines only
	// partly on screen)

	draw_line(left, top, right, top, linestate);
	draw_line(left, top, left, bottom, linestate);
	draw_line(right, top, right, bottom, linestate);
	draw_line(left, bottom, right, bottom, linestate);
	draw_fill_rect(left, top, right, bottom, fillstate);
}

void CLCDDisplay::draw_polygon(int num_vertices, int *vertices, int state)
{
	// coordinate checking in draw_pixel (-> you can draw polygons only
	// partly on screen)

	int i;
	for (i = 0; i < num_vertices - 1; i++)
	{
		draw_line(vertices[(i << 1) + 0],
			vertices[(i << 1) + 1],
			vertices[(i << 1) + 2],
			vertices[(i << 1) + 3],
			state);
	}

	draw_line(vertices[0],
		vertices[1],
		vertices[(num_vertices << 1) - 2],
		vertices[(num_vertices << 1) - 1],
		state);
}

struct rawHeader
{
	uint8_t width_lo;
	uint8_t width_hi;
	uint8_t height_lo;
	uint8_t height_hi;
	uint8_t transp;
} __attribute__((packed));

bool CLCDDisplay::paintIcon(std::string filename, int x, int y, bool invert)
{
	struct rawHeader header;
	uint16_t         stride;
	uint16_t         height;
	unsigned char   *pixpos;

	int _fd;
	filename = iconBasePath + filename;

	_fd = open(filename.c_str(), O_RDONLY);

	if (_fd == -1)
	{
		printf("\nerror while loading icon: %s\n\n", filename.c_str());
		return false;
	}

	read(_fd, &header, sizeof(struct rawHeader));

	stride = ((header.width_hi << 8) | header.width_lo) >> 1;
	height = (header.height_hi << 8) | header.height_lo;

	unsigned char pixbuf[200];
	while (height-- > 0)
	{
		read(fd, &pixbuf, stride);
		pixpos = (unsigned char *) &pixbuf;
		for (int count2 = 0; count2 < stride; count2++)
		{
			unsigned char compressed = *pixpos;

			draw_point(x + (count2 << 1), y, ((((compressed & 0xf0) >> 4) != header.transp) ^ invert) ? PIXEL_ON : PIXEL_OFF);
			draw_point(x + (count2 << 1) + 1, y, (((compressed & 0x0f)       != header.transp) ^ invert) ? PIXEL_ON : PIXEL_OFF);

			pixpos++;
		}
		y++;
	}

	close(_fd);
	return true;
}

void CLCDDisplay::clear_screen()
{
	memset(_buffer, 0, raw_buffer_size);
}

void CLCDDisplay::dump_screen(raw_display_t *screen)
{
	memmove(*screen, _buffer, raw_buffer_size);
}

void CLCDDisplay::load_screen_element(const raw_lcd_element_t *element, int left, int top)
{
	unsigned int i;

	//if (element->buffer)
	//	for (i = 0; i < element->header.height; i++)
	//		memmove(_buffer+((top+i) * xres)+left, element->buffer+(i * element->header.width), element->header.width);
	//
	if ((element->buffer) && (element->header.height <= yres - top))
		for (i = 0; i < min(element->header.height, yres - top); i++)
			memmove(_buffer + ((top + i) * xres) + left, element->buffer + (i * element->header.width), min(element->header.width, xres - left));
	//
}

void CLCDDisplay::load_screen(const raw_display_t *const screen)
{
	raw_lcd_element_t element;
	element.buffer_size = raw_buffer_size;
	element.buffer = *screen;
	element.header.width = xres;
	element.header.height = yres;
	load_screen_element(&element, 0, 0);
}

bool CLCDDisplay::load_png_element(const char *const filename, raw_lcd_element_t *element)
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
	png_byte    *fbptr;
	FILE        *fh;
	bool         ret_value = false;

	if ((fh = fopen(filename, "rb")))
	{
		if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
		{
			if (!(info_ptr = png_create_info_struct(png_ptr)))
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
			else
			{
#if (PNG_LIBPNG_VER < 10500)
				if (!(setjmp(png_ptr->jmpbuf)))
#else
				if (!setjmp(png_jmpbuf(png_ptr)))
#endif
				{
					unsigned int lcd_height = yres;
					unsigned int lcd_width = xres;

					png_init_io(png_ptr, fh);

					png_read_info(png_ptr, info_ptr);
					png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

					if (
						((color_type == PNG_COLOR_TYPE_PALETTE) ||
							((color_type & PNG_COLOR_MASK_COLOR) == PNG_COLOR_TYPE_GRAY)) &&
						(bit_depth  <= 8) &&
						(width      <= lcd_width) &&
						(height     <= lcd_height)
					)
					{
						printf("[CLCDDisplay] %s %s %dx%dx%d, type %d\n", __FUNCTION__, filename, width, height, bit_depth, color_type);
						element->header.width = width;
						element->header.height = height;
						element->header.bpp = bit_depth;
						if (!element->buffer)
						{
							element->buffer_size = width * height;
							element->buffer = new unsigned char[element->buffer_size];
							lcd_width = width;
							lcd_height = height;
						}

						memset(element->buffer, 0, element->buffer_size);

						png_set_packing(png_ptr); /* expand to 1 byte blocks */

						if (color_type == PNG_COLOR_TYPE_PALETTE)
							png_set_expand(png_ptr);

						if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
							png_set_expand(png_ptr);

						if (color_type & PNG_COLOR_MASK_COLOR)
#if (PNG_LIBPNG_VER < 10200)
							png_set_rgb_to_gray(png_ptr);
#else
							png_set_rgb_to_gray(png_ptr, 1, NULL, NULL);
#endif

						number_passes = png_set_interlace_handling(png_ptr);
						png_read_update_info(png_ptr, info_ptr);

						if (width == png_get_rowbytes(png_ptr, info_ptr))
						{
							ret_value = true;

							for (pass = 0; pass < number_passes; pass++)
							{
								fbptr = (png_byte *)element->buffer;
								for (i = 0; i < element->header.height; i++)
								{
									//fbptr = row_pointers[i];
									//png_read_rows(png_ptr, &fbptr, NULL, 1);

									png_read_row(png_ptr, fbptr, NULL);
									/* if the PNG is smaller, than the display width... */
									if (width < lcd_width)	/* clear the area right of the PNG */
										memset(fbptr + width, 0, lcd_width - width);
									fbptr += lcd_width;
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

bool CLCDDisplay::load_png(const char *const filename)
{
	raw_lcd_element_t element;
	element.buffer_size = raw_buffer_size;
	element.buffer = _buffer;
	return load_png_element(filename, &element);
}

bool CLCDDisplay::dump_png_element(const char *const filename, raw_lcd_element_t *element)
{
	png_structp  png_ptr;
	png_infop    info_ptr;
	unsigned int i;
	png_byte    *fbptr;
	FILE        *fp;
	bool         ret_value = false;

	/* create file */
	fp = fopen(filename, "wb");
	if (!fp)
		printf("[CLCDDisplay] File %s could not be opened for writing\n", filename);
	else
	{
		/* initialize stuff */
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

		if (!png_ptr)
			printf("[CLCDDisplay] png_create_write_struct failed\n");
		else
		{
			info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr)
				printf("[CLCDDisplay] png_create_info_struct failed\n");
			else
			{
				if (setjmp(png_jmpbuf(png_ptr)))
					printf("[CLCDDisplay] Error during init_io\n");
				else
				{
					unsigned int lcd_height = yres;
					unsigned int lcd_width = xres;

					png_init_io(png_ptr, fp);


					/* write header */
					if (setjmp(png_jmpbuf(png_ptr)))
						printf("[CLCDDisplay] Error during writing header\n");

					png_set_IHDR(png_ptr, info_ptr, element->header.width, element->header.height,
						element->header.bpp, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
						PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

					png_write_info(png_ptr, info_ptr);


					/* write bytes */
					if (setjmp(png_jmpbuf(png_ptr)))
					{
						printf("[CLCDDisplay] Error during writing bytes\n");
						return ret_value;
					}

					ret_value = true;

					fbptr = (png_byte *)element->buffer;
					for (i = 0; i < element->header.height; i++)
					{
						png_write_row(png_ptr, fbptr);
						fbptr += lcd_width;
					}

					/* end write */
					if (setjmp(png_jmpbuf(png_ptr)))
					{
						printf("[CLCDDisplay] Error during end of write\n");
						return ret_value;
					}

					png_write_end(png_ptr, NULL);
				}
			}
		}
		fclose(fp);
	}

	return ret_value;
}

bool CLCDDisplay::dump_png(const char *const filename)
{
	raw_lcd_element_t element;
	element.buffer_size = raw_buffer_size;
	element.buffer = _buffer;
	element.header.width = xres;
	element.header.height = yres;
	element.header.bpp = 8;
	return dump_png_element(filename, &element);
}
