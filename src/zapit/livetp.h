/*
 * livetp.h - whether the live channel's tuner stands on its transponder
 *
 * Copyright (C) 2026 Tuxbox-Project (live tuner taken)
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
 * A stream can hold the live frontend on another transponder, for
 * instance one set up while the box was in standby. A zap to the live
 * channel then finds no frontend, the live channel stays what it was, and
 * the box shows nothing. A zap to the channel shown is otherwise taken for
 * one that changes nothing; this decides, from values read off the box,
 * when it is not.
 */

#ifndef __livetp_h__
#define __livetp_h__

#include <stdint.h>

namespace livetp
{

/* Whether the live channel is displaced from its tuner: it is a broadcast
   channel (web: false), its frontend is there (present) and tuned, but to
   another transponder than the channel's. NVOD channels compare the lower
   32 bits of both transponder ids, as the frontend does when it decides
   whether a zap changes the transponder. A frontend without signal, or no
   frontend at all, is not displaced. */
bool displaced(bool web, bool present, bool tuned, uint64_t fe_tp, uint64_t channel_tp, bool nvod);

} // namespace livetp

#endif
