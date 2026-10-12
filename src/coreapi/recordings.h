/*
 * recordings.h - recordings, their files, and what is being written
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions)
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

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

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

/* Begins a recording of the channel the box is showing, as the record key does.

   Conflict BoxInStandby while the box is in standby and wake was not given (the
   loop would switch it on to record), Conflict RecordingRunning when that channel
   is already being recorded, the shift aside, since the loop would start nothing.
   Both are read before the command goes out and can be overtaken. Ok says the
   command reached the loop and no more: watch list() for the row. */
Result<void> startInstant(bool wake);

/* A directory a recording timer can be given to write into. */
struct Folder
{
	std::string path;      // absolute, without a trailing slash
	bool        root;      // one the box is set up with rather than one found below it
	bool        present;   // there now; a root on a disc that is not attached is not
};

struct FolderList
{
	std::vector<Folder> items;
	bool                partial;   // the list stopped at kMaxFolders
};

const size_t kMaxFolders = 256;
// As deep as the movie browser looks for films (src/gui/moviebrowser/mb_storage.cpp).
const int kFolderDepth = 10;

/* The roots first: the box's record directory and the movie browser's own
   directories, in that order, each once and whether or not the browser is set to
   scan it. Then the directories below each root in turn, down to kFolderDepth
   levels, each followed by its own and siblings sorted by name. Hidden directories
   are left out and links below a root are not followed.

   The walk runs on a thread of its own, one at a time, and this waits for it a few
   seconds at most (recordings_internal.h): a share that stopped answering holds
   that thread and not the caller. A walk cut short, by that wait, by kMaxFolders,
   or by how many entries it may read in all, says so with partial; the roots are
   always in the list, those not reached as not present. */
FolderList folders();

} // namespace recordings
} // namespace coreapi

#endif
