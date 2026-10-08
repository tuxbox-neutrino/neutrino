/*
 * test_screenshot.cpp - tests for screenshots
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes, tests)
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
#include "coreapi/box/displaypicture.h"
#include "coreapi/base/result.h"
#include "coreapi/osd.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

using namespace coreapi;

namespace
{

bool exists(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

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

std::string readFrom(int fd)
{
	std::string out;
	char buf[256];
	ssize_t n = 0;
	while ((n = ::read(fd, buf, sizeof(buf))) > 0)
		out.append(buf, (size_t) n);
	return out;
}

std::string readPath(const std::string &path)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return "(no file)";
	const std::string out = readFrom(fd);
	::close(fd);
	return out;
}

// A descriptor closed whichever line a case leaves through.
struct OpenFile
{
	int fd;
	explicit OpenFile(const std::string &path) : fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC)) {}
	~OpenFile() { if (fd >= 0) ::close(fd); }

	private:
		OpenFile(const OpenFile &);
		OpenFile &operator=(const OpenFile &);
};

} // namespace

/* The lock is given back before the server opens the file and sends it, so the
   next capture may run while an answer is still going out. That answer has to
   keep the picture it opened, whole. */
TEST_CASE("a picture still being sent is not rewritten by the next capture", "[screenshot]")
{
	FakeScreenshotSource source;
	source.content = "the first picture";
	InstalledScreenshotSource installed(&source);

	const std::string path = osd::screenshot(true, true, PictureFormat::Jpeg).value();
	OpenFile sending(path);
	REQUIRE(sending.fd >= 0);

	source.content = "the second";
	REQUIRE(osd::screenshot(true, true, PictureFormat::Jpeg).value() == path);

	REQUIRE(readFrom(sending.fd) == "the first picture");
	REQUIRE(readPath(path) == "the second");
}

/* While a capture is being written the name still holds the last whole picture,
   and it holds the new one once the capture has finished. */
TEST_CASE("the name holds the last whole picture while the next is taken", "[screenshot]")
{
	HeldScreenshotSource source;
	source.content = "the picture before";
	source.let_go = true;
	InstalledScreenshotSource installed(&source);
	const std::string path = osd::screenshot(true, true, PictureFormat::Png).value();
	REQUIRE(readPath(path) == "the picture before");

	source.let_go = false;
	source.inside = false;
	source.content = "the picture after";
	std::string during;
	{
		CaptureInFlight next(source);
		REQUIRE(source.waitInside(kHeldMs));
		during = readPath(path);
	}
	REQUIRE(during == "the picture before");
	REQUIRE(readPath(path) == "the picture after");
}
/* The two drivers run side by side, so the list is built from what each says
   and the settings of the one that has them. */
struct DisplaysOn
{
	FakeScreenshotSource source;
	FakeSettingsSource settings;
	InstalledScreenshotSource installed_source;
	InstalledSettingsSource installed_settings;

	DisplaysOn() : installed_source(&source), installed_settings(&settings)
	{
		source.display_status = Status::Ok;
		settings.ints["glcd_enable"] = 1;
	}
};

static std::vector<std::string> namesOf(const std::vector<osd::Display> &all)
{
	std::vector<std::string> names;
	for (size_t i = 0; i < all.size(); ++i)
		names.push_back(all[i].name);
	return names;
}

TEST_CASE("no display running lists none", "[screenshot]")
{
	DisplaysOn on;
	Result<std::vector<osd::Display> > got = osd::displays();
	REQUIRE(got.ok());
	REQUIRE(got.value().empty());
}

TEST_CASE("graphlcd is listed when it is drawing, lcd4linux only with both settings", "[screenshot]")
{
	DisplaysOn on;
	on.source.live_displays.insert("graphlcd");
	on.source.live_displays.insert("lcd4linux");

	// The picture file is there but the settings do not ask for it.
	REQUIRE(namesOf(osd::displays().value()) == std::vector<std::string>(1, "graphlcd"));

	on.settings.ints["lcd4l_support"] = 2;
	REQUIRE(namesOf(osd::displays().value()) == std::vector<std::string>(1, "graphlcd"));

	on.settings.ints["lcd4l_screenshots"] = 1;
	std::vector<std::string> both;
	both.push_back("graphlcd");
	both.push_back("lcd4linux");
	REQUIRE(namesOf(osd::displays().value()) == both);
	REQUIRE(osd::displays().value()[1].title == "LCD4Linux");

	// Settings on and no picture being written is not a display.
	on.source.live_displays.erase("lcd4linux");
	REQUIRE(namesOf(osd::displays().value()) == std::vector<std::string>(1, "graphlcd"));
}

/* The driver keeps its bitmap once it has drawn, so a box that switched GraphLCD
   off still answers that it is live. Only the setting tells the two apart. */
TEST_CASE("graphlcd switched off is not listed and not captured", "[screenshot]")
{
	DisplaysOn on;
	on.source.live_displays.insert("graphlcd");
	REQUIRE(namesOf(osd::displays().value()) == std::vector<std::string>(1, "graphlcd"));

	on.settings.ints["glcd_enable"] = 0;
	REQUIRE(osd::displays().value().empty());
	Result<std::string> taken = osd::displayScreenshot("graphlcd");
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().status == Status::NotSupported);
	REQUIRE(on.source.display_shots == 0u);
}

static void writeText(const std::string &path, const std::string &body)
{
	FILE *f = fopen(path.c_str(), "wb");
	REQUIRE(f != NULL);
	fwrite(body.data(), 1, body.size(), f);
	fclose(f);
}

static std::string readText(const std::string &path)
{
	std::string out;
	FILE *f = fopen(path.c_str(), "rb");
	if (f == NULL)
		return out;
	char buf[512];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	fclose(f);
	return out;
}

// The one part of the display path that runs on the generic build.
TEST_CASE("the lcd4linux picture is copied whole and and a display is live only while its program runs", "[screenshot]")
{
	const std::string tag = std::to_string((long) getpid());
	const std::string source = "/tmp/ni-lcd4l-test-" + tag + ".png";
	const std::string target = "/tmp/ni-lcd4l-test-copy-" + tag + ".png";
	unlink(source.c_str());
	unlink(target.c_str());

	// Not written: not a display, and a copy says so.
	REQUIRE_FALSE(displayPictureLive(source.c_str(), true));
	REQUIRE_FALSE(displayPictureLive(source.c_str(), false));
	REQUIRE(copyDisplayPicture(source.c_str(), target) == Status::NotSupported);
	REQUIRE_FALSE(exists(target));

	std::string body(10000, 'x');
	body[0] = 'P';
	body[9999] = 'Z';
	writeText(source, body);
	REQUIRE(displayPictureLive(source.c_str(), true));
	REQUIRE_FALSE(displayPictureLive(source.c_str(), false));
	REQUIRE(copyDisplayPicture(source.c_str(), target) == Status::Ok);
	REQUIRE(readText(target) == body);
	REQUIRE_FALSE(exists(target + ".tmp"));

	// A picture that has stood still for hours is still live while the program runs.
	struct utimbuf old = { time(NULL) - 3600, time(NULL) - 3600 };
	REQUIRE(utime(source.c_str(), &old) == 0);
	REQUIRE(displayPictureLive(source.c_str(), true));

	// Nowhere to write: a fault, and nothing left under either name.
	REQUIRE(copyDisplayPicture(source.c_str(), "/nonexistent-dir/x.png") == Status::Internal);
	unlink(source.c_str());
	unlink(target.c_str());
}

TEST_CASE("the display is a capture of its own", "[screenshot]")
{
	DisplaysOn on;
	on.source.live_displays.insert("graphlcd");

	Result<std::string> taken = osd::displayScreenshot("graphlcd");
	REQUIRE(taken.ok());
	REQUIRE(on.source.display_shots == 1u);
	REQUIRE(on.source.last_display == "graphlcd");
	// The screen was not asked, which is what makes these two captures and not
	// one with a flag.
	REQUIRE(on.source.screen_shots == 0u);
	REQUIRE(exists(taken.value()));
	// A file of its own as well, or one would be answered as the other.
	REQUIRE(taken.value() != osd::screenshot(true, true, PictureFormat::Png).value());

	on.source.live_displays.insert("lcd4linux");
	on.settings.ints["lcd4l_support"] = 1;
	on.settings.ints["lcd4l_screenshots"] = 1;
	Result<std::string> second = osd::displayScreenshot("lcd4linux");
	REQUIRE(second.ok());
	REQUIRE(second.value() != taken.value());
}

TEST_CASE("a display that is not running says so rather than failing", "[screenshot]")
{
	DisplaysOn on;

	Result<std::string> taken = osd::displayScreenshot("graphlcd");
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().status == Status::NotSupported);
	REQUIRE(taken.error().code == ErrorCode::DisplayNotCaptured);
	REQUIRE(on.source.display_shots == 0u);

	// Running, but switched off by the settings.
	on.source.live_displays.insert("lcd4linux");
	taken = osd::displayScreenshot("lcd4linux");
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().code == ErrorCode::DisplayNotCaptured);
}

TEST_CASE("a display nobody has is told apart from one that is off", "[screenshot]")
{
	DisplaysOn on;
	Result<std::string> taken = osd::displayScreenshot("tft");
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().status == Status::NotFound);
	REQUIRE(taken.error().code == ErrorCode::NoSuchDisplay);
	// Not a path either.
	REQUIRE(osd::displayScreenshot("../etc/passwd").error().code == ErrorCode::NoSuchDisplay);
}
