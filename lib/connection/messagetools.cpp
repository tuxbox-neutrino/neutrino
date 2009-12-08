/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/messagetools.cpp,v 1.1 2002/12/07 13:37:06 thegoodguy Exp $
 *
 * tools used in clientlib <-> daemon communication - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "messagetools.h"

size_t write_length_field (unsigned char * buffer, unsigned int length)
{
        if (length < 128)
        {
                buffer[0] = length;
                return 1;
        }
        else
        {
                unsigned int pos = 0;
                unsigned int shiftby = 8;
                unsigned char length_field_size = 1;

                while ((length >> shiftby) != 0)
                {
                        length_field_size++;
                        shiftby += 8;
                }

                buffer[pos++] = ((1 << 7) | length_field_size);

                while (shiftby != 0)
                {
			shiftby -= 8;
                        buffer[pos++] = length >> shiftby;
                }
                return pos;
        }

}

unsigned int parse_length_field (const unsigned char * buffer)
{
	unsigned char size_indicator = (buffer[0] >> 7) & 0x01;
	unsigned int length_value = 0;

	if (size_indicator == 0)
	{
		length_value = buffer[0] & 0x7F;
	}

	else if (size_indicator == 1)
	{
		unsigned char length_field_size = buffer[0] & 0x7F;
		unsigned int i;

		for (i = 0; i < length_field_size; i++)
		{
			length_value = (length_value << 8) | buffer[i + 1];
		}
	}

	return length_value;
}

size_t get_length_field_size (const unsigned int length)
{
	if (length < 0x80)
		return 0x01;

	if (length < 0x100)
		return 0x02;

	if (length < 0x10000)
		return 0x03;

	if (length < 0x1000000)
		return 0x04;
	
	return 0x05;
}
