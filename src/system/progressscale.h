/*
 * progressscale.h - how far a step is, and where that puts the whole
 *
 * Copyright (C) 2026 NI-Team
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
 */

#ifndef __progressscale__
#define __progressscale__

#include <stdint.h>

/* The share of a step that is done, in percent.

   A size that is not known yet counts as nothing done: curl reports a total of
   0 until the header is in, and dividing by it used to put whatever the
   conversion made of infinity on the bar. More done than the size counts as
   all of it: a read that skips a bad block takes a whole erase block off what
   is left, and the percent used to run past 100 and wrap in a char. */
static inline unsigned int progressPercent(int64_t done, int64_t total)
{
	if (total <= 0 || done <= 0)
		return 0;
	if (done >= total)
		return 100;
	return (unsigned int)(done * 100 / total);
}

/* Where the global bar stands when a step that runs it from begin to end is
   percent done. An end before begin leaves the bar where it is rather than
   taking it back, so a step that starts late does not undo what came before. */
static inline unsigned int progressBetween(unsigned int begin, int end, unsigned int percent)
{
	if (end < 0 || (unsigned int) end <= begin)
		return begin;
	if (percent > 100)
		percent = 100;
	return begin + ((unsigned int) end - begin) * percent / 100;
}

#endif
