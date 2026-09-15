/*
 * decryption.cpp - what the running channel is being descrambled with
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

#include <config.h>

#include "decryption.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"

#include <sys/stat.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace decryption
{

namespace
{

OpenThreads::Mutex &pathMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

/* The file a softcam writes while it is answering for the running channel.

   It comes from outside this program and is not an interface anything here
   owns: a softcam runs beside the box's own program, hands the control word to
   the descrambler through a socket this program never sees, and writes this
   file so the screens have something to read. It removes it again when it
   stops descrambling, which is what makes an absent file mean what it means.

   There is nothing better to read. Nothing inside this program is told that a
   control word arrived; the channel stack knows which systems a channel is
   scrambled under and which daemon the box was asked to start, and neither of
   those says a word is being delivered for what is on the screen now. The
   box's own screens read this same file for this same question. */
std::string &pathStore()
{
	static std::string s("/tmp/ecm.info");
	return s;
}

} // namespace

void setEcmInfoPath(const std::string &path)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(pathMutex());
	pathStore() = path;
}

std::string ecmInfoPath()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(pathMutex());
	return pathStore();
}

Result<State> state()
{
	State out;

	/* Present and not empty, which is the pair the box's own screens ask for.
	   A softcam that has opened the file and written nothing into it yet has
	   answered for nothing, and a length of nought reads the same as a file
	   that is not there. */
	struct stat st;
	out.softcam = ::stat(ecmInfoPath().c_str(), &st) == 0 &&
		      S_ISREG(st.st_mode) && st.st_size > 0;

	unsigned slots = 0;
	Status s = systemSource().ciSlotCount(slots);
	if (s != Status::Ok)
		return fail(s, ErrorCode::DecryptionUnreadable,
			    "the common interface of this box could not be read");

	/* Stops at the first module it finds, because the answer is whether there
	   is one and not which slot it is in. A box with no slot at all walks none
	   of this and is answered no module, which is the truth about it rather
	   than a reading that failed. */
	bool seated = false;
	for (unsigned i = 0; i < slots && !seated; i++)
	{
		bool here = false;
		s = systemSource().ciModulePresent(i, here);
		if (s != Status::Ok)
			return fail(s, ErrorCode::DecryptionUnreadable,
				    "a common interface slot of this box could not be read");
		seated = here;
	}

	bool in_use = false;
	s = systemSource().ciInUse(in_use);
	if (s != Status::Ok)
		return fail(s, ErrorCode::DecryptionUnreadable,
			    "whether this box descrambles through its common interface could not be read");

	/* Both, because either on its own answers a different question. A module
	   nobody is sending the channel through descrambles nothing, and a box set
	   to descramble through one with the slot empty descrambles nothing
	   either. */
	out.ci_module = seated && in_use;
	return ok(out);
}

} // namespace decryption
} // namespace coreapi
