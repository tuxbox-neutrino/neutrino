/*
 * timers.h - timers: reading, adding, changing, removing
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

#ifndef __coreapi_timers_h__
#define __coreapi_timers_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <stdint.h>

namespace coreapi
{
namespace timers
{

// In the order the daemon keeps them, which is by the id it handed out. Timers
// it has finished with are not among them.
Result<TimerList> list();

// The id the daemon gave the new timer, which is never zero.
//
// InvalidArgument for a timer that cannot do anything: a kind the daemon does
// not build, a recording or a zap with no channel, a recording that ends no
// later than it starts, and a one-off whose moment has already passed. That last
// rule is asked only of a one-off, because a repeating timer whose first
// occurrence is behind us is what the daemon reschedules.
//
// An immediate recording is filed as an ordinary recording whose start has come,
// because that is the only shape of it the daemon runs: handed the kind itself
// it makes a row that reports itself as running and records nothing. So a read
// of the timer afterwards answers the kind it was built as, and the recording
// begins at the daemon's next pass rather than at the moment of the request.
//
// Remotebox is among the kinds it does not build: it is the label another box's
// timer wears here, so making one would put a row in this box's timer file that
// names a recording this box will never take.
Result<uint32_t> create(const TimerInfo &t);

// The kind and the channel stay where they are, the daemon's protocol carrying
// no way to move them, so the kind is read here only to pick which exchange
// carries the change. NotFound for an id the daemon does not hold, asked before
// the change rather than reported by it.
Result<void> modify(const TimerInfo &t);

// NotFound for an id the daemon does not hold, asked before the removal because
// the daemon acknowledges a removal with nothing at all.
//
// Ok means the daemon no longer lists the timer, which is read back rather than
// assumed. It does not mean the timer file has been rewritten: the daemon does
// that from its own thread on its own schedule, up to twenty seconds later.
Result<void> remove(uint32_t id);

} // namespace timers
} // namespace coreapi

#endif
