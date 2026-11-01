/*
 * waitslice.h - a bounded wait taken in slices
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 * zapit waits for a tuner in poll() and sleeps, up to seconds at a time. A
 * stop of the program has to be seen within a slice of such a wait, not at
 * its end; the whole wait still lasts as long as it did.
 */

#ifndef __zapit_waitslice_h__
#define __zapit_waitslice_h__

#include <stdint.h>

namespace waitslice
{

// The longest single wait, in milliseconds.
const int kSliceMs = 100;

/* The next wait of a wait that lasts total ms, elapsed ms of which have
   passed: never longer than slice, never longer than what is left, and 0
   once nothing is left. An elapsed time below 0 - a clock read that went
   backwards - counts as none. */
int nextSliceMs(int64_t total, int64_t elapsed, int slice);

} // namespace waitslice

#endif
