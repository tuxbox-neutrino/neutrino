/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/src/driver/slotbuffer.c,v 1.1 2004/06/03 09:51:54 thegoodguy Exp $
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

/* Note: this slotbuffer "wastes" one slot */

#include "slotbuffer.h"
#include <stdlib.h>

slotbuffer_t * slotbuffer_construct(const size_t size)
{
	slotbuffer_t * slotbuffer;

	slotbuffer = malloc(sizeof(slotbuffer_t));

	slotbuffer->buffer        = malloc(size);
	slotbuffer->size          = size;
	slotbuffer->read_pointer  = 0;
	slotbuffer->write_pointer = 0;

	return slotbuffer;
}

void slotbuffer_destruct(slotbuffer_t * const slotbuffer)
{
	free(slotbuffer->buffer);
	free(slotbuffer);
}

size_t slotbuffer_remaining_read_space(slotbuffer_t * const slotbuffer)
{
	ssize_t delta;

	delta = slotbuffer->write_pointer;
	delta -= slotbuffer->read_pointer;

	return (delta >= 0) ? (size_t)delta : (size_t)(delta + slotbuffer->size);
}

size_t slotbuffer_remaining_write_space(slotbuffer_t * const slotbuffer)
{
	ssize_t delta;

	delta = slotbuffer->read_pointer;
	delta -= slotbuffer->write_pointer;

	return ((delta > 0) ? (size_t)delta : (size_t)(delta + slotbuffer->size)) - 1;
}

size_t slotbuffer_remaining_continuous_read_space(slotbuffer_t * const slotbuffer)
{
	ssize_t delta;
	size_t  read_pointer;

	delta         = slotbuffer->write_pointer;
	read_pointer  = slotbuffer->read_pointer;
	delta        -= read_pointer;

	return (delta >= 0) ? (size_t)delta : (size_t)(slotbuffer->size - read_pointer);
}

size_t slotbuffer_remaining_continuous_write_space(slotbuffer_t * const slotbuffer)
{
	ssize_t delta;
	size_t  write_pointer;

	delta          = slotbuffer->read_pointer;

	if (delta == 0)
		return (slotbuffer->size - slotbuffer->write_pointer - 1);

	write_pointer  = slotbuffer->write_pointer;
	delta         -= write_pointer;

	return (delta > 0) ? (size_t)(delta - 1) : (size_t)(slotbuffer->size - write_pointer);
}

void slotbuffer_advance_read_pointer(slotbuffer_t * const slotbuffer, const size_t delta)
{
	slotbuffer->read_pointer += delta;
	if (slotbuffer->read_pointer >= slotbuffer->size)
		slotbuffer->read_pointer = 0;
}

void slotbuffer_advance_write_pointer(slotbuffer_t * const slotbuffer, const size_t delta)
{
	slotbuffer->write_pointer += delta;
	if (slotbuffer->write_pointer >= slotbuffer->size)
		slotbuffer->write_pointer = 0;
}
