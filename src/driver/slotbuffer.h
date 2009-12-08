#ifndef __slotbuffer_h__
#define __slotbuffer_h__

/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/src/driver/slotbuffer.h,v 1.1 2004/06/03 09:51:54 thegoodguy Exp $
 *
 * (C) 2004 by thegoodguy <thegoodguy@berlios.de>
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

#include <sys/types.h>

typedef struct
{
	unsigned char * buffer;
	size_t          size;
	volatile size_t read_pointer;
	volatile size_t write_pointer;
} slotbuffer_t;

slotbuffer_t * slotbuffer_construct                       (size_t         const size                          );
void           slotbuffer_destruct                        (slotbuffer_t * const slotbuffer                    );
size_t         slotbuffer_remaining_read_space            (slotbuffer_t * const slotbuffer                    );
size_t         slotbuffer_remaining_write_space           (slotbuffer_t * const slotbuffer                    );
size_t         slotbuffer_remaining_continuous_read_space (slotbuffer_t * const slotbuffer                    );
size_t         slotbuffer_remaining_continuous_write_space(slotbuffer_t * const slotbuffer                    );
void           slotbuffer_advance_read_pointer            (slotbuffer_t * const slotbuffer, size_t const delta);
void           slotbuffer_advance_write_pointer           (slotbuffer_t * const slotbuffer, size_t const delta);

#endif
