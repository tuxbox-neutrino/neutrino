/*
 * unicable_check.h - two tuners on one unicable cable on the same user band
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

#ifndef __zapit_unicable_check_h__
#define __zapit_unicable_check_h__

#include <vector>

/*
 * Every tuner on a unicable cable asks the LNB for a user band of its own: an
 * address (SCR) the LNB answers to, and the frequency it then puts that band
 * on. Address and frequency are set apart from each other, and nothing ties
 * them to what the LNB has: two tuners on one band, or a frequency that
 * belongs to another band, take the picture from each other or get none, and
 * nothing in the box says why. This finds the first case, which is the one
 * the setup can see.
 */

typedef struct unicable_use {
	int tuner;	/* what the caller names the tuner by */
	int cable;	/* tuners with the same value share one cable */
	int scr;	/* user band address, from 0 */
	int freq;	/* user band frequency in MHz, 0 if not set */
} unicable_use_t;

typedef enum {
	UNICABLE_CLASH_NONE,
	UNICABLE_CLASH_SCR,	/* same address on the same cable */
	UNICABLE_CLASH_FREQ	/* same frequency on the same cable */
} unicable_clash_t;

/* The first other tuner in uses on the cable of me with the same address, or
 * failing that with the same frequency; its index goes to other. A frequency
 * of 0 is one nobody set, and clashes with nothing. me itself may be in uses,
 * found by its tuner, and is skipped. */
static inline unicable_clash_t unicableClash(const std::vector<unicable_use_t> &uses,
					     const unicable_use_t &me, int &other)
{
	other = -1;
	for (size_t i = 0; i < uses.size(); i++) {
		if (uses[i].tuner == me.tuner || uses[i].cable != me.cable)
			continue;
		if (uses[i].scr == me.scr) {
			other = (int) i;
			return UNICABLE_CLASH_SCR;
		}
	}
	if (me.freq == 0)
		return UNICABLE_CLASH_NONE;
	for (size_t i = 0; i < uses.size(); i++) {
		if (uses[i].tuner == me.tuner || uses[i].cable != me.cable)
			continue;
		if (uses[i].freq == me.freq) {
			other = (int) i;
			return UNICABLE_CLASH_FREQ;
		}
	}
	return UNICABLE_CLASH_NONE;
}

#endif /* __zapit_unicable_check_h__ */
