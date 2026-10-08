/*
 * boxdefaults.h - which box models fall back to a different default
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

#ifndef __COREAPI_BOXDEFAULTS_H__
#define __COREAPI_BOXDEFAULTS_H__

#include <config.h>

/* The groups of models the loader gives a different fallback, each written as
   it is in the loader, so the two can be read side by side.

   Here and not in the tables that use them: the build check compiles a table
   under every combination of the conditions it names, which stops at eight, and
   the models of these groups alone are more. A header it includes is compiled
   once under each model on its own, which is what an arm that is a single model
   needs. */

namespace coreapi
{
namespace boxdefault
{

#if BOXMODEL_HD51
const bool kFavoritesIsVideo = true;
#else
const bool kFavoritesIsVideo = false;
#endif

#if BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE || BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS
const bool kTimeshiftIsNone = true;
#else
const bool kTimeshiftIsNone = false;
#endif

#if BOXMODEL_VUPLUS_ALL
const bool kVuPlus = true;
#else
const bool kVuPlus = false;
#endif

#if BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K || BOXMODEL_HD61
const bool kTvRadioKeyIsTv = true;
#else
const bool kTvRadioKeyIsTv = false;
#endif

// Models whose remote has one key for play and pause.
#if BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE
const bool kPlayPauseKey = true;
#else
const bool kPlayPauseKey = false;
#endif

#if BOXMODEL_E4HDULTRA
const bool kZappingModeTwo = true;
#else
const bool kZappingModeTwo = false;
#endif

#if BOXMODEL_VUUNO4KSE
const bool kScrollSpeedOne = true;
#else
const bool kScrollSpeedOne = false;
#endif

#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUDUO4KSE || BOXMODEL_VUULTIMO4K
const bool kScrollSpeedTwo = true;
#else
const bool kScrollSpeedTwo = false;
#endif

} // namespace boxdefault
} // namespace coreapi

#endif
