/*
 * picture_internal.h - what the video decoder's numbers mean
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef COREAPI_BOX_PICTURE_INTERNAL_H
#define COREAPI_BOX_PICTURE_INTERNAL_H

#include "coreapi/base/types.h"

#include <string>

namespace coreapi
{
namespace internal
{

/* The decoder answers the aspect and the frame rate as positions in two lists,
   the ones the box's own stream information names them by
   (src/gui/streaminfo2.cpp). A position outside a list is a value the box has
   no name for, and answered as that rather than guessed at. */
inline std::string aspectName(int position)
{
	static const char *const names[] = { "", "4:3", "14:9", "16:9", "20:9" };
	if (position < 0 || position >= (int) (sizeof(names) / sizeof(names[0])))
		return "";
	return names[position];
}

inline double framesPerSecond(int position)
{
	static const double rates[] = { 23.976, 24, 25, 29.97, 30, 50, 59.94, 60 };
	if (position < 0 || position >= (int) (sizeof(rates) / sizeof(rates[0])))
		return 0;
	return rates[position];
}

/* The numbers as the decoder answers them, made into what a reader is told.
   A decoder with no size has nothing on show. 1088 lines is how some decoders
   report a picture of 1080, the eight below it being padding the coding needs
   and nobody sees; the box's own stream information says 1080 as well. */
inline void fillPicture(int width, int height, int rate, int aspect, PictureInfo &out)
{
	out = PictureInfo();
	if (width <= 0 || height <= 0)
		return;
	out.shown = true;
	out.width = width;
	out.height = (height == 1088) ? 1080 : height;
	out.frame_rate = framesPerSecond(rate);
	out.aspect = aspectName(aspect);
}

} // namespace internal
} // namespace coreapi

#endif
