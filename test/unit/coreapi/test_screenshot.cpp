/*
 * test_screenshot.cpp - tests for screenshots
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

#include "support/catch.hpp"
#include "support/fakes.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/osd.h"

#include <atomic>
#include <string>

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

void restFor(int ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000 * 1000;
	nanosleep(&ts, NULL);
}

/* How long a capture below is allowed to sit inside the box. A ceiling and not a
   wait for a signal, because the whole point of the case that uses it is a build
   that queues the second capture behind this one: such a build leaves this
   thread waiting on a thread that is waiting on it, and a suite that hangs says
   nothing about anything. Nothing waits this out when the refusal comes back. */
const int kHeldMs = 3000;

/* A capture that does not leave the box until it is let go. Counted before it is
   held, so a capture that reached the box and is still in there is one the
   counter has already seen. */
struct HeldScreenshotSource : public FakeScreenshotSource
{
	std::atomic<bool> inside;
	std::atomic<bool> let_go;

	HeldScreenshotSource() : inside(false), let_go(false) {}

	coreapi::Status captureScreen(bool osd, bool video, coreapi::PictureFormat format,
				      const std::string &path)
	{
		const coreapi::Status s =
			FakeScreenshotSource::captureScreen(osd, video, format, path);
		inside = true;
		for (int waited = 0; waited < kHeldMs && !let_go; waited += 10)
			restFor(10);
		return s;
	}

	// Whether a capture got as far as the box, rather than whether one was
	// started: a thread that never ran leaves the same silence as one that is
	// being kept out.
	bool waitInside(int budget_ms)
	{
		for (int waited = 0; waited < budget_ms && !inside; waited += 10)
			restFor(10);
		return inside;
	}
};

/* One capture on a thread of its own, let go and joined from the destructor
   whichever line the case leaves through: a check that fails unwinds past
   whatever came after it, and a case that left this thread sitting in the box
   would take the ceiling above with it for nothing. */
struct CaptureInFlight
{
	HeldScreenshotSource &source;
	pthread_t thread;
	bool started;
	std::atomic<bool> took;

	explicit CaptureInFlight(HeldScreenshotSource &s)
		: source(s), thread(), started(false), took(false)
	{
		started = pthread_create(&thread, NULL, &run, this) == 0;
	}

	~CaptureInFlight()
	{
		source.let_go = true;
		if (started)
			pthread_join(thread, NULL);
	}

	static void *run(void *arg)
	{
		CaptureInFlight *f = static_cast<CaptureInFlight *>(arg);
		f->took = osd::screenshot(true, true, PictureFormat::Png).ok();
		return NULL;
	}

	private:
		CaptureInFlight(const CaptureInFlight &);
		CaptureInFlight &operator=(const CaptureInFlight &);
};

};

} // namespace

	InstalledScreenshotSource installed(&source);

{
	HeldScreenshotSource source;
	source.let_go = true;
	InstalledScreenshotSource installed(&source);
		REQUIRE(source.waitInside(kHeldMs));