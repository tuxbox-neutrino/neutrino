/*
 * picturesource_real.cpp - what the box's video decoder is showing
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/base/deps.h"
#include "coreapi/box/picture_internal.h"

#include <hardware/video.h>

extern cVideo *videoDecoder;

namespace coreapi
{

namespace
{

/* The decoder is asked the way the box's own stream information asks it
   (src/gui/streaminfo2.cpp): blanked, it shows nothing, whatever it decoded
   last; otherwise its size, its frame rate and its aspect. Asked from the
   web server's thread as that screen and the old control API ask it from
   theirs: three reads of what the driver holds, nothing set. */
class RealPictureSource : public PictureSource
{
	public:
		Status picture(PictureInfo &out) const
		{
			out = PictureInfo();
			// made while the box starts, before the web server answers
			if (videoDecoder == NULL)
				return Status::NotSupported;
			if (videoDecoder->getBlank())
				return Status::Ok;
			int width = 0, height = 0, rate = -1;
			videoDecoder->getPictureInfo(width, height, rate);
			internal::fillPicture(width, height, rate, videoDecoder->getAspectRatio(), out);
			return Status::Ok;
		}
};

RealPictureSource g_real_picture;

} // anonymous namespace

void installRealPictureSource() { setPictureSource(&g_real_picture); }

} // namespace coreapi
