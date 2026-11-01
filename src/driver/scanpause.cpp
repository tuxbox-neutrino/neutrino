/*
 * scanpause.cpp - when the EPG scan stops for a stream and when it goes on
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
 */

#include "scanpause.h"

namespace scanpause
{

bool resumeNow(bool paused, bool scanning)
{
	return paused && scanning;
}

BackZap onBackZap(bool tuned, bool busy)
{
	if (busy)
		return Pause;
	return tuned ? Read : Skip;
}

EitComplete onEitComplete(bool own, bool disturbed, bool busy, int rereads)
{
	if (!own || !(disturbed || busy))
		return Count;
	return rereads < kMaxReReads ? ReadAgain : CountDisturbed;
}

int rereadsAtHandOver(bool same, bool disturbed, int rereads)
{
	if (!same)
		return 0;
	return disturbed ? rereads + 1 : rereads;
}

} // namespace scanpause
