/*
 * recordings.h - recordings, their files, and what is being written
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

#ifndef __coreapi_recordings_h__
#define __coreapi_recordings_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <stdint.h>

namespace coreapi
{
namespace recordings
{

/* In the order of the numbers that name them. A box recording nothing answers
   an empty list, recording nothing being a thing this can say, and several at
   once is the ordinary case: this box records as many channels as its tuners
   and its own ceiling allow, and the shift it keeps of what it is showing is
   one of them.

   How large each file is is read here rather than by whatever holds the
   recordings, because asking the filesystem from inside the lock the box's own
   thread needs back would hold that thread for as long as a disc takes to
   answer. A file that cannot be reached leaves its size unstated. */
Result<RecordingList> list();

/* NotFound for a number nothing is being recorded under, asked before the
   request goes out, so ending a recording twice is answered as ending one that
   is not there. The number cannot come to name a different recording in
   between: the daemon that hands them out counts up and never gives one out
   twice while it runs.

   Ok says the box was asked and not that anything has stopped: what ends a
   recording is the loop that writes it, reached by way of the timer daemon, and
   nothing here can wait for either. */
Result<void> stop(uint32_t id);

/* Begins the shift of whatever the box is showing, and ends it.

   The shift is a recording like any other in the list above, so ending it could
   also be asked for by its number. It has two calls of its own because a caller
   that means the shift has no number in hand, and because ending it this way is
   what the box does itself: the number ends the writing, and this ends the
   shift, which also stops the box from starting the next one on its own.

   Conflict for a start while the box is already keeping one, NotFound for an
   end while it is keeping none. Both are read before the command goes out and
   both can be overtaken; what settles either for good is the loop, where the
   answer and the act are not separated by a thread. Ok says the command reached
   the loop's socket and no more. */
Result<void> startTimeshift();
Result<void> stopTimeshift();

} // namespace recordings
} // namespace coreapi

#endif
