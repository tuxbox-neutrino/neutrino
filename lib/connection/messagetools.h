/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/messagetools.h,v 1.1 2002/12/07 13:37:06 thegoodguy Exp $
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

#ifndef __messagetools_h__
#define __messagetools_h__

#include <malloc.h>

size_t write_length_field (unsigned char * buffer, const unsigned int length);
size_t get_length_field_size (const unsigned int length);
unsigned int parse_length_field (const unsigned char * buffer);


#endif /* __messagetools_h__ */
