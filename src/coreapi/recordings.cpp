/*
 * recordings.cpp - recordings, their files, and what is being written
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

#include "recordings.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"

#include <utility>

#include <sys/stat.h>

namespace coreapi
{
namespace recordings
{

namespace
{

// As in every other read here, the status comes from the source so that one
// cause cannot become two answers on the way out. Only the code and the
// wording are this layer's.
Failure listUnreadable(Status s)
{
	return fail(s, ErrorCode::RecordingListUnavailable,
		    "what the box is recording could not be read");
}

/* Left unstated rather than guessed at when the file cannot be measured, because
   nought is a real size: a recording begun a moment ago has written nothing yet,
   and a reader handed nought for a disc that went away would be told that
   instead of being told nothing.

   Only a plain file is measured. A path that has become a directory or a device
   is not the recording it was. */
void measure(RecordingInfo &r)
{
	r.size = 0;
	r.size_known = false;
	if (r.path.empty())
		return;

	struct stat st;
	if (stat(r.path.c_str(), &st) != 0)
		return;
	if (!S_ISREG(st.st_mode))
		return;

	r.size = (uint64_t) st.st_size;
	r.size_known = true;
}

/* The running recordings, for the three calls that each have to know something
   about them before they act. Shared so that a list that could not be read is
   never mistaken for a box recording nothing, which is the one reading that
   would turn every refusal below into a success. Gone through whole because two
   of the three ask about the set and not about a member. */
Status running(RecordingList &out)
{
	return recordingSource().list(out);
}

// Whether one of them is the shift the box keeps of what it is showing. There
// is at most one: the box starts the next only once the last has ended.
bool anyTimeshift(const RecordingList &all)
{
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].timeshift)
			return true;
	}
	return false;
}

bool holds(const RecordingList &all, uint32_t id)
{
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].id == id)
			return true;
	}
	return false;
}

} // anonymous namespace

Result<RecordingList> list()
{
	RecordingList out;
	Status s = running(out);
	if (s != Status::Ok)
		return listUnreadable(s);

	for (size_t i = 0; i < out.size(); i++)
		measure(out[i]);

	return ok(std::move(out));
}

Result<void> stop(uint32_t id)
{
	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (!holds(all, id))
		return fail(Status::NotFound, ErrorCode::NoSuchRecording,
			    "the box is recording nothing under that number");

	s = recordingSource().stop(id);
	if (s != Status::Ok)
		return fail(s, ErrorCode::RecordingNotStopped,
			    "the box did not take the request to stop recording");
	return ok();
}

Result<void> startTimeshift()
{
	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	/* Asked here as well as on the loop, although only the loop's answer can be
	   the last word. Without it a caller asking twice is told twice that the box
	   was asked, and learns nothing about why the second one changed nothing. */
	if (anyTimeshift(all))
		return fail(Status::Conflict, ErrorCode::TimeshiftRunning,
			    "the box is already keeping a shift");

	return postEvent(BoxEvent::StartTimeshift);
}

Result<void> stopTimeshift()
{
	RecordingList all;
	Status s = running(all);
	if (s != Status::Ok)
		return listUnreadable(s);
	if (!anyTimeshift(all))
		return fail(Status::NotFound, ErrorCode::NoTimeshift,
			    "the box is keeping no shift");

	return postEvent(BoxEvent::StopTimeshift);
}

} // namespace recordings
} // namespace coreapi
