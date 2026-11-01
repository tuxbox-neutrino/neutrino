/*
 * recordtuner.h - the frontend a recording takes
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
 * The decisions CRecordManager::CutBackNeutrino takes about the live
 * frontend, as plain functions over what it reads off the box, so that
 * they can be run where no tuner is.
 *
 * The live channel id does not say where the live frontend stands. A wake
 * from standby whose zap found no frontend leaves both as they were, and an
 * EPG scan in standby sets the live channel id to the channel it reads
 * without moving the live frontend. So whether the live frontend carries a
 * channel is read off the frontend: tuned, and on the channel's transponder.
 * The frontend's own sameTsidOnid() does not answer that: it is true for a
 * frontend whose transponder id is still 0, which it is after opening and
 * until it is tuned.
 */

#ifndef __recordtuner_h__
#define __recordtuner_h__

#include <stdint.h>

namespace recordtuner
{

/* Whether the live frontend carries the transponder channel_tp: there is
   one, it is tuned, and to that transponder. */
bool liveServes(bool present, bool tuned, uint64_t tsid_onid, uint64_t channel_tp);

/* Whether the recording's frontend needs a full zap, which changes what the
   box shows: it is the live frontend, the live channel is a broadcast one,
   and the live frontend does not carry the recording's transponder. A
   recording on another frontend, or beside a web channel, is only tuned. */
bool fullZap(bool live_is_frontend, bool live_web, bool live_serves);

} // namespace recordtuner

#endif
