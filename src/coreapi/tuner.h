/*
 * tuner.h - signal readings, and the frontends that take them
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

#ifndef __coreapi_tuner_h__
#define __coreapi_tuner_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

namespace coreapi
{
namespace tuner
{

/* What the frontend carrying the picture is measuring, raw and with the scale
   the readings were taken against.

   NotSupported where nothing is carrying a picture, which is the answer a box
   with no tuner gives and is not a fault: there is a difference between a box
   this cannot be asked of and a box where asking went wrong, and only the
   second is worth reporting as something broken. */
Result<SignalInfo> live();

// Every tuner the box has. A box with none answers with an empty list rather
// than with a refusal, because having no tuner is a thing this can say.
Result<FrontendList> frontends();

/* Puts the tuner stack down, brings it back up and tunes the running channel
   again, which is what somebody does about a picture that has gone while the
   box carried on as though it had not.

   Answers as soon as the box has been told and not when a picture is back: what
   the tuning comes to is read off the reading above, which is the one thing
   that can say whether there is a signal. So this says the reset was asked for
   and nothing more.

   Nothing is asked about what the box is doing first. A recording keeps its
   frontend, the channel daemon holding it open for as long as one is running,
   and everything else this touches is the live picture, which is what the
   caller is asking to have put right. */
Result<void> reset();

} // namespace tuner
} // namespace coreapi

#endif
