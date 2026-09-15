/*
 * timers.cpp - timers: reading, adding, changing, removing
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

#include "timers.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/eventbus.h"

#include <utility>

#include <timerdclient/timerdtypes.h>

namespace coreapi
{
namespace timers
{

// As in the channel and event reads, the status always comes from the source, so
// one cause cannot become two answers on the way out.
//
// The rules below live here rather than in a caller because the timer daemon
// applies none of them: it takes a recording with no channel and one that ends
// before it begins, writes both to its file and fires them.

namespace
{

// Remotebox is not one of them: nothing stores a timer of that kind, it is the
// label another box's timer wears once this box has fetched it over the network.
bool creatableType(int t)
{
	switch (t)
	{
		case (int) TimerType::Shutdown:
		case (int) TimerType::Zapto:
		case (int) TimerType::Standby:
		case (int) TimerType::Record:
		case (int) TimerType::Remind:
		case (int) TimerType::Sleeptimer:
		case (int) TimerType::ExecPlugin:
		case (int) TimerType::ImmediateRecord:
			return true;
		default:
			return false;
	}
}

// The kinds that do nothing at all without one. A recording of channel zero is
// a recording of nothing, and the daemon makes it all the same.
bool needsChannel(int t)
{
	return t == (int) TimerType::Record ||
	       t == (int) TimerType::ImmediateRecord ||
	       t == (int) TimerType::Zapto;
}

// The kinds that write to a disc, which is what the rule about a duration is
// asked of. Both of them, because the one below turns into the other and a rule
// asked of only one would be a rule the other slipped past.
bool isRecording(int t)
{
	return t == (int) TimerType::Record ||
	       t == (int) TimerType::ImmediateRecord;
}

/* An immediate recording is not a kind the daemon can be handed. It files one as
   already running, which is what its one real sender needs: the box itself makes
   such a row after it has already begun a recording, so that something carries
   the end time. The daemon's loop fires a timer only as it enters the running
   state, so a row that arrives in that state is never fired, and handed straight
   through the kind makes a row that reports itself as running and records
   nothing.

   Firing it instead is not open either: the sender above has already started
   that recording, and a second start under the identifier the first one holds
   makes a second instance the box cannot reach.

   So what a caller means by the name is built out of the kind the daemon does
   run: a recording whose start has come, fired at the daemon's next pass. */
int builtAs(int type)
{
	if (type == (int) TimerType::ImmediateRecord)
		return (int) TimerType::Record;
	return type;
}

// Where a caller builds a moment out of separate fields it has no seconds to
// give, and both places that do it zero them, so a start anywhere inside the
// current minute is one the caller means as now. A caller that hands over a
// moment whole keeps its seconds and is held to them.
time_t startOfMinute(time_t t) { return t - (t % 60); }

Status daemonHolds(uint32_t id, bool &out)
{
	TimerList held;
	Status s = timerSource().list(held);
	if (s != Status::Ok)
		return s;
	out = false;
	for (size_t i = 0; i < held.size(); i++)
	{
		if (held[i].id == id)
		{
			out = true;
			break;
		}
	}
	return Status::Ok;
}

// Shared by the three entry points that read the list to answer about one
// timer, so that a list that cannot be read never reads as a timer that is not
// there.
Failure listUnreadable(Status s)
{
	return fail(s, ErrorCode::TimerListUnavailable, "the timer list could not be read");
}

} // anonymous namespace

Result<TimerList> list()
{
	TimerList out;
	Status s = timerSource().list(out);
	if (s != Status::Ok)
		return listUnreadable(s);
	return ok(std::move(out));
}

namespace
{
/* Nothing the message loop sends says that a timer was added, changed or taken
   away, so the announcement is made here, on the far side of a change the box
   has confirmed it took. The id is what the announcement carries: a reader that
   keeps timers reads that one back, and one that keeps a list reads the list. */
void announceChange(uint32_t id)
{
	Event e;
	e.type = EventType::TimerChanged;
	e.value = (int) id;
	EventBus::instance().publish(e);
}
} // anonymous namespace

Result<uint32_t> create(const TimerInfo &t)
{
	if (!creatableType(t.type))
		return fail(Status::InvalidArgument, ErrorCode::NoSuchTimerType,
			    "the box does not make timers of that kind");

	if (needsChannel(t.type) && t.channel_id == 0)
		return fail(Status::InvalidArgument, ErrorCode::TimerWithoutChannel,
			    "this kind of timer needs a channel");

	if (isRecording(t.type) && t.stop <= t.start)
		return fail(Status::InvalidArgument, ErrorCode::RecordingWithoutDuration,
			    "a recording has to end after it begins");

	// Asked of a one-off only. A repeating timer whose first occurrence is
	// behind us is the ordinary way to enter one, and the daemon moves it to
	// its next occurrence.
	if (t.repeat == (int) CTimerd::TIMERREPEAT_ONCE)
	{
		time_t now = 0;
		Status s = timerSource().now(now);
		if (s != Status::Ok)
			return fail(s, ErrorCode::ClockUnavailable,
				    "the box could not read the time");
		if (t.start < startOfMinute(now))
			return fail(Status::InvalidArgument, ErrorCode::TimerInThePast,
				    "a timer that runs once cannot begin before now");
	}

	TimerInfo asked = t;
	asked.type = builtAs(t.type);

	uint32_t new_id = 0;
	Status s = timerSource().add(asked, new_id);
	if (s == Status::Conflict)
		return fail(s, ErrorCode::TimerExists,
			    "the box already has a timer like this one");
	if (s != Status::Ok)
		return fail(s, ErrorCode::TimerNotCreated,
			    "the box did not take the timer");
	announceChange(new_id);
	return ok(new_id);
}

Result<void> modify(const TimerInfo &t)
{
	if (t.id == 0)
		return fail(Status::InvalidArgument, ErrorCode::TimerWithoutId,
			    "there is no timer to change without an id");

	if (t.type == (int) TimerType::Record && t.stop <= t.start)
		return fail(Status::InvalidArgument, ErrorCode::RecordingWithoutDuration,
			    "a recording has to end after it begins");

	bool held = false;
	Status s = daemonHolds(t.id, held);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (!held)
		return fail(Status::NotFound, ErrorCode::NoSuchTimer,
			    "no timer with that id");

	s = timerSource().modify(t);
	if (s != Status::Ok)
		return fail(s, ErrorCode::TimerNotChanged,
			    "the box did not take the change");
	announceChange(t.id);
	return ok();
}

Result<void> remove(uint32_t id)
{
	bool held = false;
	Status s = daemonHolds(id, held);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (!held)
		return fail(Status::NotFound, ErrorCode::NoSuchTimer,
			    "no timer with that id");

	s = timerSource().remove(id);
	if (s != Status::Ok)
		return fail(s, ErrorCode::TimerNotRemoved,
			    "the box did not take the removal");

	// Read back, because the removal itself is answered with nothing at all:
	// without this an Ok would say only that a message was written to a socket,
	// which is true even for a daemon that has stopped reading it.
	s = daemonHolds(id, held);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (held)
		return fail(Status::Internal, ErrorCode::TimerStillThere,
			    "the box still has the timer");
	announceChange(id);
	return ok();
}

} // namespace timers
} // namespace coreapi
