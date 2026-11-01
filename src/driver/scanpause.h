/*
 * scanpause.h - when the EPG scan stops for a stream and when it goes on
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
 * A stream comes before the EPG scan. The scan does not zap while a
 * recording runs, a stream runs or a stream is being set up; it pauses, and
 * goes on when the stream server reports that the last stream ended. A
 * transponder whose read a stream interrupted stays in the scan's list and
 * is read again once no stream runs, a few times in a row at most: when a
 * stream's hold took the frontend before the scan's own zap was done, and
 * when a stream started, or a setup that asked zapit for a frontend
 * started none, while sectionsd read the transponder. sectionsd reports a
 * pass over its filters complete for its current service whatever the
 * frontend stood on meanwhile, so that report does not count.
 */

#ifndef __scanpause_h__
#define __scanpause_h__

namespace scanpause
{

/* Whether the end of a stream lets the scan go on: only a scan that
   stopped for a recording or a stream, and only while the scan runs in the
   box's mode at all (scanning). A scan that is reading a transponder is
   left to it, and a scan that is off asks nothing of zapit. */
bool resumeNow(bool paused, bool scanning);

enum BackZap
{
	Read,    // the transponder is read
	Pause,   // the scan waits for the streams, the transponder kept
	Skip     // the transponder could not be tuned: the next one
};

/* What the scan does when zapit has done its zap. busy: a stream runs or
   is being set up. */
BackZap onBackZap(bool tuned, bool busy);

enum EitComplete
{
	Count,          // the transponder was read: off the list, on to the next
	ReadAgain,      // kept in the list, read again once no stream runs
	CountDisturbed  // disturbed again after kMaxReReads re-reads: off the list as read
};

/* How often in a row a transponder is handed over to be read again after
   a stream came into its read. A client that opens short streams more
   often than one pass takes would otherwise keep the transponder unread,
   the scan from ending and zapit awake in standby. Reports sectionsd
   repeats while a stream keeps the scan paused start no re-read and do not
   count towards it. */
const int kMaxReReads = 3;

/* What the scan does when sectionsd reports a pass complete. own: the
   report names the transponder the scan handed over; disturbed: a stream
   started since; busy: a stream runs or is being set up now; rereads: how
   often in a row that transponder was handed over again already after a
   disturbed read. The scan hands nothing over while a stream runs or is
   set up, so one that runs at the report came after the hand-over, also
   when its start is not seen yet. */
EitComplete onEitComplete(bool own, bool disturbed, bool busy, int rereads);

/* The re-reads in a row when the scan hands a transponder over. same: it
   is the transponder handed over last; disturbed: a stream came into that
   read; rereads: the count so far. Another transponder starts at 0, and
   the same one counts one more only when its read was disturbed. A re-read
   is counted here and not per disturbed report: sectionsd repeats its
   pass, about once an hour, while a long stream keeps the scan paused. */
int rereadsAtHandOver(bool same, bool disturbed, int rereads);

} // namespace scanpause

#endif
