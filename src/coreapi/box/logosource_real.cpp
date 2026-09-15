/*
 * logosource_real.cpp - channel logo files as the box stores them
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

#include "coreapi/base/deps.h"

#include <stdint.h>

#include <driver/pictureviewer/pictureviewer.h>

extern CPictureViewer *g_PicViewer;

namespace coreapi
{

namespace
{

/* Its own translation unit, so that the picture layer and everything the search
   reaches through it are only pulled into a binary that installs this.

   Nothing here decides where a picture may be. The box already has one search
   that knows the directories it keeps pictures in, the four extensions one may
   carry and the several names one file may go under, and a second copy written
   here would be a second answer to the question which file a channel is shown
   with. */
class RealLogoSource : public LogoSource
{
	public:
		Status logoFor(ChannelId id, const std::string &name, std::string &path) const
		{
			// Before the box has built it there is nothing to search with, and
			// that is this layer's own start order rather than the box saying
			// the channel has no picture.
			if (g_PicViewer == NULL)
				return Status::Internal;

			/* No width and no height wanted, which is what the two nulls say.
			   Asking for either opens the file and decodes its header, and what
			   this answers is where the file is. */
			std::string found;
			if (!g_PicViewer->GetLogoName((uint64_t) id, name, found, NULL, NULL))
				return Status::NotFound;

			path = found;
			return Status::Ok;
		}
};

RealLogoSource g_real_logo;

} // anonymous namespace

void installRealLogoSource() { setLogoSource(&g_real_logo); }

} // namespace coreapi
