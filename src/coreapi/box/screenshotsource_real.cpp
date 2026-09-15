/*
 * screenshotsource_real.cpp - screenshots taken from the running framebuffer
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

#include <stdio.h>

#include <string>

#ifdef SCREENSHOT
// The capture's own header names a channel identifier and a file, and states
// neither: it was only ever reached through a consumer that had both already.
#include <zapit/types.h>
#include <driver/screenshot.h>
#endif

#ifdef ENABLE_GRAPHLCD
#include <driver/glcd/glcd.h>
#endif

namespace coreapi
{

namespace
{

#ifdef SCREENSHOT
/* Which of the capture's own encoders writes a form. Its third one is not
   reachable from here: that one is a bitmap with nothing done to it, megabytes
   for a screen this size, and every caller above this asks for a picture over a
   socket. */
CScreenShot::screenshot_format_t encoderFor(PictureFormat f)
{
	return (f == PictureFormat::Jpeg) ? CScreenShot::FORMAT_JPG
					  : CScreenShot::FORMAT_PNG;
}
#endif

/* Its own translation unit, so that the video decoder, the framebuffer and
   whatever drives the display on the front of the box are pulled in only by a
   binary that installs this. Without that, every build that wants this layer and
   none of those would either fail to link or drag the whole driver in. */
class RealScreenshotSource : public ScreenshotSource
{
	public:
		Status captureScreen(bool osd, bool video, PictureFormat format, const std::string &path)
		{
#ifdef SCREENSHOT
			/* On the stack and not on the heap: the call below is the whole
			   of the capture and has finished by the time it answers, so
			   there is nothing left for the object to outlive. */
			CScreenShot shot(path, encoderFor(format));
			shot.EnableOSD(osd);
			shot.EnableVideo(video);
			// A capture that failed wrote no file, or wrote one that is not a
			// picture; either way there is nothing at that name to hand on.
			return shot.StartSync() ? Status::Ok : Status::Internal;
#else
			// A box whose platform has no way of reading its own screen. It is
			// an answer about the box rather than a fault.
			(void) osd;
			(void) video;
			(void) format;
			(void) path;
			return Status::NotSupported;
#endif
		}

		Status captureDisplay(const std::string &path)
		{
#ifdef ENABLE_GRAPHLCD
			cGLCD *display = cGLCD::getInstance();
			// Built when the box starts and only where one is configured, so
			// nothing here is a fault either: the box has no such display.
			if (display == NULL || display->bitmap == NULL)
				return Status::NotSupported;
			return display->dumpBuffer((fb_pixel_t *) display->bitmap->Data(),
						   cGLCD::PNG, path.c_str())
				? Status::Ok : Status::Internal;
#else
			// Most boxes have no second display and no build made without it
			// has one, which is the common answer here rather than the rare
			// one.
			(void) path;
			return Status::NotSupported;
#endif
		}
};

RealScreenshotSource g_real_screenshot;

} // anonymous namespace

void installRealScreenshotSource() { setScreenshotSource(&g_real_screenshot); }

} // namespace coreapi
