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
#include "coreapi/box/displaypicture.h"

#include <stdio.h>

#include <system/helpers.h>

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

// Where LCD4Linux writes the picture of its display.
const char LCD4LINUX_PICTURE[] = "/tmp/lcd4linux.png";

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

		bool displayLive(const std::string &name)
		{
			if (name == "lcd4linux")
				// The test Neutrino's own LCD4Linux driver makes of the process.
				return displayPictureLive(LCD4LINUX_PICTURE, getpidof("lcd4linux") > 0);
#ifdef ENABLE_GRAPHLCD
			// Looked up and never made: a request must not start a display, and
			// a Respawn in progress has none to find.
			if (name == "graphlcd")
			{
				cGLCD *display = cGLCD::peekInstance();
				return display != NULL && display->bitmap != NULL;
			}
#endif
			return false;
		}

		Status captureDisplay(const std::string &name, const std::string &path)
		{
			if (name == "lcd4linux")
				return copyDisplayPicture(LCD4LINUX_PICTURE, path);
#ifdef ENABLE_GRAPHLCD
			if (name == "graphlcd")
			{
				cGLCD *display = cGLCD::peekInstance();
				if (display == NULL || display->bitmap == NULL)
					return Status::NotSupported;
				return display->dumpBuffer((fb_pixel_t *) display->bitmap->Data(),
							   cGLCD::PNG, path.c_str())
					? Status::Ok : Status::Internal;
			}
#endif
			// A build made without the driver has no such display, which is the
			// common answer here and not a fault.
			return Status::NotSupported;
		}
};

RealScreenshotSource g_real_screenshot;

} // anonymous namespace

void installRealScreenshotSource() { setScreenshotSource(&g_real_screenshot); }

} // namespace coreapi
