/*
 * decryption.h - what the running channel is being descrambled with
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

#ifndef __coreapi_decryption_h__
#define __coreapi_decryption_h__

#include "coreapi/base/result.h"

#include <string>

namespace coreapi
{
namespace decryption
{

/* The two ways this box can be getting a control word, as two facts about the
   moment they are read in.

   Neither says the channel is scrambled at all. That is a property of the
   channel and is answered where the channel is, so that one fact has one
   place to be wrong in: both of these are false on a channel in the clear and
   both are false on a scrambled channel nothing can open, and only the
   channel's own answer tells those apart. */
struct State
{
	// A softcam is answering for what the box is showing right now.
	bool softcam;
	// A module is seated and the box is sending the channel through it.
	bool ci_module;

	State() : softcam(false), ci_module(false) {}
};

/* Read afresh on every call, both of them.

   A module is pulled and pushed while the box runs, so a reading taken once
   and kept answers for the slot as it was when the program started for as long
   as the program lives, and the display that reads this is the one a person
   watches while they push the card in.

   Costs a stat and a walk over the slots, no wait and no round trip, which is
   what lets a display ask for it as often as it redraws. */
Result<State> state();

/* Where the file a softcam leaves behind is looked for.

   Settable for the reason the process table root is: a case has no softcam to
   run, and the machine running the suite has a directory of that name already
   holding whatever else is on it. */
void setEcmInfoPath(const std::string &path);
std::string ecmInfoPath();

} // namespace decryption
} // namespace coreapi

#endif
