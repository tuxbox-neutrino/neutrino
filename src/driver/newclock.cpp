/*      
        LCD-Daemon  -   DBoxII-Project

        Copyright (C) 2001 Steffen Hehn 'McClean',
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


#include <config.h>
#include <cstdio>
#include <driver/newclock.h>

static bool	time_digits[24*32 * 10];
static bool	days[24*16 * 7];
static bool	date_digits[16*16 * 10];
static bool	months[32*16 * 12];

static bool	signs[] = {1, 1, 1, 1,
			   1, 1, 1, 1,
			   1, 1, 1, 1,
			   1, 1, 1, 1,
			   
			   1, 1, 1, 0,
			   1, 1, 1, 0,
			   0, 1, 1, 0,
			   1, 1, 0, 0,
			   
			   1, 1, 1, 0,
			   1, 1, 1, 0,
			   1, 1, 1, 0,
			   0, 0, 0, 0};

void loadSkin(char * const filename, char * const backup_filename, const unsigned int modify_char_filename, const unsigned int modify_char_backup_filename, bool * const dest, const unsigned int count, const unsigned int width, const unsigned int height, const char * const name)
{
	FILE * fd;
	int row, bit;
	char * file;
	unsigned int i ,byte, digit_pos;
	unsigned char BMPWidth;
	unsigned char BMPHeight;
	char line_buffer[4];
	file = filename;
	digit_pos = modify_char_filename;

	for (i = 0; i < count; i++)
	{
		bool retried = false;
	retry:
		if ((fd = fopen(file, "rb")) == 0)
		{
			printf("[lcdd] %s-skin not found (%s) -> using default...\n", name, file);
			file = backup_filename;
			digit_pos = modify_char_backup_filename;
			i = 0;
			if (!retried) {
				retried = true;
				goto retry;
			}
			break;
		}

		fseek(fd, 0x12, SEEK_SET);
		fread(&BMPWidth, 1, 1, fd);
		fseek(fd, 0x16, SEEK_SET);
		fread(&BMPHeight, 1, 1, fd);

		if ((BMPWidth > width) || (BMPHeight > height))
		{
			printf("[lcdd] %s-skin not supported -> using default...\n", name);
			fclose(fd);
			file = backup_filename;
			digit_pos = modify_char_backup_filename;
			i = 0;
			if (!retried) {
				retried = true;
				goto retry;
			}
		}

		fseek(fd, 0x3E, SEEK_SET);

		for (row = height - 1; row >= 0; row--)
		{
			fread(&line_buffer, 1, sizeof(line_buffer), fd); /* width must not be greater than 32 */

			for (byte = 0; byte < (width >> 3); byte++) /* width must be multiple of 8 */
			{
				for (bit = 7; bit >= 0; bit--)
				{
					dest[(7 - bit) + (byte << 3) + (row + i * height) * width] = line_buffer[byte] & (1 << bit);
				}
			}
		}

		fclose(fd);

		file[digit_pos]++;
	}
}

void InitNewClock(void)
{
	char filename_usr[] = CONFIGDIR "/lcdd/clock/t_a.bmp";
	char filename_std[] = DATADIR   "/lcdd/clock/t_a.bmp";

	loadSkin(filename_usr, filename_std, sizeof(filename_usr) - 6, sizeof(filename_std) - 6, time_digits, 10, 24, 32, "time");

	filename_usr[sizeof(filename_usr) - 8] = 'w';
	filename_std[sizeof(filename_std) - 8] = 'w';
	filename_usr[sizeof(filename_usr) - 6] = 'a';
	filename_std[sizeof(filename_std) - 6] = 'a';
	loadSkin(filename_usr, filename_std, sizeof(filename_usr) - 6, sizeof(filename_std) - 6, days, 7, 24, 16, "weekday");

	filename_usr[sizeof(filename_usr) - 8] = 'd';
	filename_std[sizeof(filename_std) - 8] = 'd';
	filename_usr[sizeof(filename_usr) - 6] = 'a';
	filename_std[sizeof(filename_std) - 6] = 'a';
	loadSkin(filename_usr, filename_std, sizeof(filename_usr) - 6, sizeof(filename_std) - 6, date_digits, 10, 16, 16, "date");

	filename_usr[sizeof(filename_usr) - 8] = 'm';
	filename_std[sizeof(filename_std) - 8] = 'm';
	filename_usr[sizeof(filename_usr) - 6] = 'a';
	filename_std[sizeof(filename_std) - 6] = 'a';
	loadSkin(filename_usr, filename_std, sizeof(filename_usr) - 6, sizeof(filename_std) - 6, months, 12, 32, 16, "month");
}

void RenderSign(CLCDDisplay* const display, int sign, int x_position, int y_position)
{
	int x, y;

	for(y = 0; y < 4; y++)
	{
		for(x = 0; x < 4; x++)
		{
			display->draw_point(x_position + x, y_position + y, signs[x + y*4 + sign*sizeof(signs)/3] ? CLCDDisplay::PIXEL_ON : CLCDDisplay::PIXEL_OFF);
		}
	}
}

void RenderTimeDigit(CLCDDisplay* const display, int digit, int position)
{
	int x, y;

	for(y = 0; y < 32; y++)
	{
		for(x = 0; x < 24; x++)
		{
			display->draw_point(position + x, 5 + y, time_digits[x + y*24 + digit*sizeof(time_digits)/10] ? CLCDDisplay::PIXEL_ON : CLCDDisplay::PIXEL_OFF);
		}
	}
}

void RenderDay(CLCDDisplay* const display, int day)
{
	int x, y;

	for(y = 0; y < 16; y++)
	{
		for(x = 0; x < 24; x++)
		{
			display->draw_point(5 + x, 43 + y, days[x + y*24 + day*sizeof(days)/7] ? CLCDDisplay::PIXEL_ON : CLCDDisplay::PIXEL_OFF);
		}
	}
}

void RenderDateDigit(CLCDDisplay* const display, int digit, int position)
{
	int x, y;

	for(y = 0; y < 16; y++)
	{
		for(x = 0; x < 16; x++)
		{
			display->draw_point(position + x, 43 + y, date_digits[x + y*16 + digit*sizeof(date_digits)/10] ? CLCDDisplay::PIXEL_ON : CLCDDisplay::PIXEL_OFF);
		}
	}
}

void RenderMonth(CLCDDisplay* const display, int month)
{
	int x, y;

	for(y = 0; y < 16; y++)
	{
		for(x = 0; x < 32; x++)
		{
			display->draw_point(83 + x, 43 + y, months[x + y*32 + month*sizeof(months)/12] ? CLCDDisplay::PIXEL_ON : CLCDDisplay::PIXEL_OFF);
		}
	}
}

void ShowNewClock(CLCDDisplay* display, int hour, int minute, int second, int day, int date, int month, bool rec)
{
	RenderTimeDigit(display, hour/10, 5);
	RenderTimeDigit(display, hour%10, 32);
	RenderTimeDigit(display, minute/10, 64);
	RenderTimeDigit(display, minute%10, 91);

	/* blink the date if recording */
	if (!rec || !(second & 1))
	{
		RenderDay(display, day);

		RenderDateDigit(display, date/10, 43);
		RenderDateDigit(display, date%10, 60);

		RenderMonth(display, month);

		RenderSign(display, 1, 31, 57);
		RenderSign(display, 2, 78, 56);
	}
	if (second % 2 == 0)
	{
	RenderSign(display, 0, 58, 15);
	RenderSign(display, 0, 58, 23);
	}
}
