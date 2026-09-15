/*
 * tuner.cpp - signal readings, and the frontends that take them
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

#include "tuner.h"
#include "coreapi/base/errors.h"

#include "coreapi/base/deps.h"

#include <neutrinoMessages.h>

#include <utility>

namespace coreapi
{
namespace tuner
{

// The status always comes from the source, so one cause cannot become two
// answers on the way out. Only the code and the wording are this layer's.

/* Which frontend a live picture would come from is an assignment the box keeps,
   and it outlives what is on the screen: it stays on the one last tuned to while
   a stream from the network plays, while a recording is watched, and while
   nothing plays at all. Answered as the picture coming from there, it names a
   tuner on a box that has no aerial on it and reads 0 percent beside it.

   So the assignment is carried through only where a picture can come from a
   tuner at all. The layer below reports the assignment as it stands, because
   that is what it can see; the question of what the box is doing with it is
   answered here, where a case can ask it. */
bool takesPictureFromTuner(int box_mode)
{
	return box_mode == NeutrinoModes::mode_tv ||
	       box_mode == NeutrinoModes::mode_radio;
}

Result<SignalInfo> live()
{
	/* The layer below measures the frontend the box would take a picture from,
	   and that assignment stands whatever is on the screen. Measured while a
	   stream from the network plays, it is a reading of a tuner nothing is
	   using, printed above a list that says the same tuner is free. The answer
	   below already exists for a box with nothing to measure, and it is the
	   right one here too. */
	int box_mode = 0;
	if (channelSource().currentMode(box_mode) == Status::Ok &&
	    !takesPictureFromTuner(box_mode))
		/* A conflict with what the box is doing and not a thing it cannot do:
		   the tuner is there and could be read, and it will be again the moment
		   somebody tunes to a channel. The status a box with no tuner at all
		   answers with stands below and says the other thing, so a caller can
		   tell asking later from never asking again. */
		return fail(Status::Conflict, ErrorCode::NoTuner,
			    "no tuner is carrying a picture");

	SignalInfo out;
	Status s = tunerSource().liveSignal(out);
	if (s == Status::NotSupported)
		return fail(s, ErrorCode::NoTuner,
			    "no tuner is carrying a picture");
	if (s != Status::Ok)
		return fail(s, ErrorCode::SignalUnavailable,
			    "the tuner could not be read");
	return ok(std::move(out));
}

Result<FrontendList> frontends()
{
	FrontendList out;
	Status s = tunerSource().frontends(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::FrontendListUnavailable,
			    "the tuners of this box could not be read");

	/* A mode that cannot be read leaves the assignment alone rather than
	   clearing it: not knowing what the box is doing is no reason to say a
	   tuner is idle. */
	int box_mode = 0;
	if (channelSource().currentMode(box_mode) == Status::Ok &&
	    !takesPictureFromTuner(box_mode))
	{
		for (size_t i = 0; i < out.size(); ++i)
			out[i].live = false;
	}
	return ok(std::move(out));
}

Result<void> reset()
{
	Status s = tunerSource().reset();
	if (s != Status::Ok)
		return fail(s, ErrorCode::TunerNotReset,
			    "the box did not put its tuners through a reset");
	return ok();
}

} // namespace tuner
} // namespace coreapi
