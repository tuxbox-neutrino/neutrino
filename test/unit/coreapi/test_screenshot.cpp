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
#include "support/httpclient.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/server.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/osd.h"
#include "coreapi/base/result.h"

#include "jsoncpp/json/json.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

// The directory the layer under test writes into. Written out here on purpose:
// what these cases are about is that the caller does not choose it, so the one
// it does choose is a thing a case states rather than reads back off the
// answer it is checking.
const char kWhereTheyGo[] = "/tmp";

bool exists(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string baseName(const std::string &path)
{
	const size_t slash = path.rfind('/');
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

/* The regular files of a directory, and deliberately not its directories. /tmp belongs
   to the whole machine, so a walk of it sees whatever else is running: the shell checks
   beside this suite each take a directory of their own there, and one turning up between
   the two walks below read as a capture left behind.

   Files are still counted whoever wrote them, which is the part this cannot rule out:
   narrowing further would mean naming the spelling the capture uses, and that is exactly
   what the case below refuses to do. */
std::set<std::string> namesIn(const char *dir)
{
	std::set<std::string> out;
	DIR *d = ::opendir(dir);
	if (d == NULL)
		return out;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		if (e->d_name[0] == '.')
			continue;
		const std::string full = std::string(dir) + "/" + e->d_name;
		struct stat st;
		std::memset(&st, 0, sizeof(st));
		// A name that cannot be looked at is left out rather than guessed at:
		// something that went away between the walk and the look was not a
		// capture this run left behind either.
		if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		out.insert(e->d_name);
	}
	::closedir(d);
	return out;
}

// How many of this process's descriptors are on one file, found by asking what
// each of them is on rather than by counting them: connections opening and
// closing move a bare number while a case runs.
size_t openCountFor(const std::string &path)
{
	DIR *d = ::opendir("/proc/self/fd");
	if (d == NULL)
		return 0;

	const std::string gone = " (deleted)";
	size_t n = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		if (e->d_name[0] == '.')
			continue;

		std::string link = "/proc/self/fd/";
		link += e->d_name;

		char target[4096];
		const ssize_t got = ::readlink(link.c_str(), target, sizeof(target) - 1);
		if (got <= 0)
			continue;

		std::string what(target, (size_t) got);
		if (what.size() > gone.size() &&
		    what.compare(what.size() - gone.size(), gone.size(), gone) == 0)
			what.erase(what.size() - gone.size());

		if (what == path)
			++n;
	}
	::closedir(d);
	return n;
}

/* Waits for the count to come back to what it should be and answers what it
   reached. The library gives the descriptor back once the client has read the
   answer, so a count taken the instant the last reply arrives can still be one
   ahead of a server that is right. One that is wrong never comes back and this
   spends the budget saying so. */
size_t waitForCount(const std::string &path, size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 20)
	{
		const size_t now = openCountFor(path);
		if (now == want)
			return now;

		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 20 * 1000 * 1000;
		nanosleep(&ts, NULL);
	}
	return openCountFor(path);
}

/* The fakes every seam the server checks before it starts, a daemon on a port
   the kernel picks, and the shipped tables in front of it. Everything is undone
   from the destructor, whichever line a case leaves through: a failed check
   unwinds past whatever came after it, and a case that left a daemon bound
   would take it with it for the rest of the run. */
struct ServingPictures
{
	InstalledDependencies wired;
	int port;

	ServingPictures() : port(0)
	{
		httpd::setRoutesForTest(NULL);

		httpd::ServerConfig c = httpd::defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		if (httpd::start(c))
			port = httpd::boundPort();
	}

	~ServingPictures()
	{
		httpd::stop();
		httpd::setRoutesForTest(NULL);
	}

	private:
		ServingPictures(const ServingPictures &);
		ServingPictures &operator=(const ServingPictures &);
};

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

::Json::Value parsed(const std::string &doc)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	::Json::Value root;
	std::string errs;
	REQUIRE(reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs));
	return root;
}

} // namespace

TEST_CASE("a capture answers the file it wrote", "[screenshot]")
{
	FakeScreenshotSource source;
	source.content = "the television";
	InstalledScreenshotSource installed(&source);

	Result<std::string> taken = osd::screenshot(true, true, PictureFormat::Png);
	REQUIRE(taken.ok());
	REQUIRE(source.screen_shots == 1u);
	REQUIRE(exists(taken.value()));
	REQUIRE(taken.value() == source.last_path);
}

/* The copied interface builds its path out of a name the caller sends, which
   is a caller writing wherever it likes the moment that name carries a slash.
   The signature here carries no name at all, and this is the rest of that: the
   answer is one file under one directory and it is the same one however the
   request was written. */
TEST_CASE("the caller does not say where the picture goes", "[screenshot]")
{
	FakeScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	const std::string first = osd::screenshot(true, true, PictureFormat::Png).value();
	const std::string again = osd::screenshot(false, true, PictureFormat::Png).value();

	REQUIRE(first == again);
	INFO(first);
	REQUIRE(first.compare(0, std::strlen(kWhereTheyGo), kWhereTheyGo) == 0);
	REQUIRE(first[std::strlen(kWhereTheyGo)] == '/');
	// Nothing below the directory either, or a name that walked out of it
	// would still read as being under it.
	REQUIRE(baseName(first).find('/') == std::string::npos);
	REQUIRE(first == std::string(kWhereTheyGo) + "/" + baseName(first));
}

TEST_CASE("which halves go in reaches the box", "[screenshot]")
{
	FakeScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	REQUIRE(osd::screenshot(true, false, PictureFormat::Png).ok());
	REQUIRE(source.last_osd);
	REQUIRE_FALSE(source.last_video);

	REQUIRE(osd::screenshot(false, true, PictureFormat::Png).ok());
	REQUIRE_FALSE(source.last_osd);
	REQUIRE(source.last_video);
}

/* A capture that came to nothing left no file, so an ok answer naming one
   would send a caller to read whatever that name carried from the time before.
   */
TEST_CASE("a capture that came to nothing is reported", "[screenshot]")
{
	FakeScreenshotSource source;
	source.screen_status = Status::Internal;
	InstalledScreenshotSource installed(&source);

	Result<std::string> taken = osd::screenshot(true, true, PictureFormat::Png);
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().status == Status::Internal);
	REQUIRE(taken.error().code == ErrorCode::ScreenNotCaptured);
	// It did reach the box, which is what tells this apart from a request
	// turned away above it.
	REQUIRE(source.screen_shots == 1u);
}

/* The whole of what one name buys. A thousand captures are a thousand files written and
   one file left, and the walk that says so is of the directory rather than of a name
   this case guessed: an implementation that numbered its captures would put its
   numbering wherever it liked.

   Every figure this rests on is shown to move first. The listing is shown to hold the
   file before the thousand start, and the box is shown to have been asked a thousand and
   one times, so neither a walk that found nothing nor a run where none was taken can
   read as a pass. */
TEST_CASE("a thousand captures do not leave a thousand files", "[screenshot]")
{
	FakeScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	const std::string path = osd::screenshot(true, true, PictureFormat::Png).value();
	REQUIRE(exists(path));

	const std::set<std::string> before = namesIn(kWhereTheyGo);
	REQUIRE(before.count(baseName(path)) == 1u);

	size_t taken = 0;
	for (int i = 0; i < 1000; ++i)
	{
		if (osd::screenshot(true, true, PictureFormat::Png).ok())
			++taken;
	}
	REQUIRE(taken == 1000u);
	REQUIRE(source.screen_shots == 1001u);

	const std::set<std::string> after = namesIn(kWhereTheyGo);
	std::vector<std::string> added;
	std::set_difference(after.begin(), after.end(), before.begin(), before.end(),
			    std::back_inserter(added));
	INFO(added.size() << " name(s) under " << kWhereTheyGo
	     << " that were not there before, first: "
	     << (added.empty() ? std::string("none") : added[0]));
	REQUIRE(added.empty());
}

/* The capture reads the framebuffer through a driver call with no deadline of
   its own, so the wait this layer used to put a second caller through was a wait
   on that call. The web server answers out of four worker threads and the page
   asks for a picture on every key: four callers queued behind one stuck read is
   a server that answers nothing at all, not even the channel list. Turned away
   instead, a stuck read costs the one worker that is in it.

   Everything this rests on is shown to move first: the first capture is shown to
   have reached the box, and the count is shown not to have moved for the second,
   so neither a thread that never ran nor a refusal invented above the box can
   read as a pass. */
TEST_CASE("a capture asked for while one is running is turned away rather than queued",
          "[screenshot]")
{
	HeldScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	bool held = false;
	bool refused = false;
	/* Neither starting value is the one this ends up checking for, so a case
	   that never reached the call cannot read as a pass. */
	Status status = Status::Ok;
	ErrorCode code = ErrorCode::DisplayNotCaptured;
	unsigned reached_box = 0;

	{
		CaptureInFlight first(source);
		held = source.waitInside(kHeldMs);

		Result<std::string> second = osd::screenshot(true, true, PictureFormat::Png);
		refused = !second.ok();
		if (refused)
		{
			status = second.error().status;
			code = second.error().code;
		}
		reached_box = source.screen_shots;
	}

	REQUIRE(held);
	REQUIRE(refused);
	// Busy and not a fault: nothing is wrong with the request and nothing is
	// wrong with the box, and what the caller does about it is come back.
	REQUIRE(status == Status::Busy);
	REQUIRE(code == ErrorCode::ScreenNotCaptured);
	// The box was asked once. A build that waited would have asked twice.
	REQUIRE(reached_box == 1u);
}

/* The refusal above is about one capture at a time and not about one for the
   life of the box, so the next ask has to go through. A trylock given back on
   one way out and not on another would pass the case above and leave every
   capture after it refused for ever. */
TEST_CASE("the next capture after a refused one is taken", "[screenshot]")
{
	HeldScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	bool refused = false;

	{
		CaptureInFlight first(source);
		REQUIRE(source.waitInside(kHeldMs));
		refused = !osd::screenshot(true, true, PictureFormat::Png).ok();
	}

	REQUIRE(refused);

	// The hold is already off: the thread above was let go and joined on its
	// way out of the block, so this one runs straight through the box.
	Result<std::string> after = osd::screenshot(true, true, PictureFormat::Png);
	REQUIRE(after.ok());
	REQUIRE(exists(after.value()));
}

TEST_CASE("the display is a capture of its own", "[screenshot]")
{
	FakeScreenshotSource source;
	source.display_status = Status::Ok;
	InstalledScreenshotSource installed(&source);

	Result<std::string> taken = osd::displayScreenshot();
	REQUIRE(taken.ok());
	REQUIRE(source.display_shots == 1u);
	// The screen was not asked, which is what makes these two captures and not
	// one with a flag.
	REQUIRE(source.screen_shots == 0u);
	REQUIRE(exists(taken.value()));
	// A file of its own as well, or one would be answered as the other.
	REQUIRE(taken.value() != osd::screenshot(true, true, PictureFormat::Png).value());
}

// Most boxes have none and no build made without it has one, so this is the
// common answer rather than the rare one, and it is an answer about the box
// rather than a fault.
TEST_CASE("a box with no such display says so rather than failing", "[screenshot]")
{
	FakeScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	Result<std::string> taken = osd::displayScreenshot();
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().status == Status::NotSupported);
	REQUIRE(taken.error().code == ErrorCode::DisplayNotCaptured);
}

/* Through the whole server and not through the router alone. A handler that
   put the file behind its answer and wrote no status on it comes back from the
   router as an answer with no code, which a case reading only what the layer
   below returned would not call wrong; the transport replaces such an answer
   with a fault about this layer, and this is where that shows.
*/
TEST_CASE("the picture goes out as the file it was written to", "[screenshot]")
{
	ServingPictures serving;
	REQUIRE(serving.port > 0);
	serving.wired.screen.content = "not really a picture, but these are its bytes";

	const testhttp::Reply r = testhttp::request(serving.port, "GET", "/api/v1/osd/screenshot");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "image/png");
	REQUIRE(r.body == serving.wired.screen.content);

	char stated[32];
	std::snprintf(stated, sizeof(stated), "%u", (unsigned) serving.wired.screen.content.size());
	REQUIRE(r.header("Content-Length") == stated);
	// Chunked would mean the client above could not have read it: it decodes
	// none, so an answer sent that way is one no case here can say anything
	// about.
	REQUIRE(r.header("Transfer-Encoding").empty());
}

TEST_CASE("the request says which halves it wants", "[screenshot]")
{
	ServingPictures serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = testhttp::request(serving.port, "GET",
						    "/api/v1/osd/screenshot?osd=0&video=1");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(serving.wired.screen.last_osd);
	REQUIRE(serving.wired.screen.last_video);
}

/* The refusal the layer below made and not one this layer invented for it. A
   platform that cannot read its own screen is what answers this, and it
   answers it as a statement about the box; a handler that dropped the refusal
   and went looking for a file that was never written would answer that the
   fault is here, which is the wrong half of the story and a different code on
   the wire. */
TEST_CASE("a capture the box could not take answers a problem and no picture", "[screenshot]")
{
	ServingPictures serving;
	REQUIRE(serving.port > 0);
	serving.wired.screen.screen_status = Status::NotSupported;

	const testhttp::Reply r = testhttp::request(serving.port, "GET", "/api/v1/osd/screenshot");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 501);
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/screen-not-captured");
}

TEST_CASE("a box with no display answers that and no picture", "[screenshot]")
{
	ServingPictures serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = testhttp::request(serving.port, "GET",
						    "/api/v1/osd/display/screenshot");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 501);
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/display-not-captured");
}

/* The answer is sent out of an open file, and every way out of the transport has to
   give that file back. A thousand of them is what makes a way out that does not say so.
   The count is of descriptors on this one file, so a connection on its way out cannot
   read as a leak, and it is shown to find the one this case holds first. */
TEST_CASE("a thousand pictures leak no descriptors", "[screenshot]")
{
	ServingPictures serving;
	REQUIRE(serving.port > 0);

	// Taken once so that the file is there, and held open here so the counter
	// has something to find.
	const std::string path = osd::screenshot(true, true, PictureFormat::Png).value();
	const int held = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	REQUIRE(held >= 0);

	const size_t before = openCountFor(path);
	REQUIRE(before == 1u);

	size_t answered = 0;
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r = testhttp::request(serving.port, "GET", "/api/v1/osd/screenshot");
		if (r.transport_ok && r.code == 200 && r.body == serving.wired.screen.content)
			++answered;
	}
	REQUIRE(answered == 1000u);

	const size_t after = waitForCount(path, before, 5000);
	::close(held);
	REQUIRE(after == before);
}

/* The route answered one form at the box's own size, so a page wanting a small
   tile paid for a whole screen written the largest of the two ways there are.
   The size is the box's and stays the box's, for the reason the call's own
   header gives; the form is the caller's. */

TEST_CASE("the form the caller asked for reaches the capture", "[screenshot]")
{
	/* Nothing the answer carries can see this. What the capture does with a
	   form is behind the encoder, so a layer that read one form and sent
	   another one down would look, from the file that came back, exactly like
	   one that read it right. */
	FakeScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	REQUIRE(osd::screenshot(true, true, PictureFormat::Jpeg).ok());
	REQUIRE(source.last_format == PictureFormat::Jpeg);

	REQUIRE(osd::screenshot(true, true, PictureFormat::Png).ok());
	REQUIRE(source.last_format == PictureFormat::Png);
}

TEST_CASE("each form is written to a name of its own", "[screenshot]")
{
	/* Two captures at once are one file, which is the whole of what one name
	   per form costs and buys. Between two callers who asked for the same form
	   that is a picture taken a moment earlier. Between two who asked for
	   different forms one name would hand a file in one form to a caller told
	   it is holding the other, which no decoder and no reader would forgive. */
	FakeScreenshotSource source;
	InstalledScreenshotSource installed(&source);

	const std::string png = osd::screenshot(true, true, PictureFormat::Png).value();
	const std::string jpeg = osd::screenshot(true, true, PictureFormat::Jpeg).value();

	REQUIRE(png != jpeg);
	// And still one name per form rather than one per call, or the free space
	// would fall by a picture for every request.
	REQUIRE(osd::screenshot(false, true, PictureFormat::Jpeg).value() == jpeg);

	// The suffix says which of the two is behind the name, because that name
	// is read by somebody who was told which form they asked for.
	REQUIRE(png.size() > 4);
	REQUIRE(png.compare(png.size() - 4, 4, ".png") == 0);
	REQUIRE(jpeg.size() > 4);
	REQUIRE(jpeg.compare(jpeg.size() - 4, 4, ".jpg") == 0);

	// Both under the one directory this layer chooses, which no caller names.
	REQUIRE(jpeg.compare(0, std::strlen(kWhereTheyGo), kWhereTheyGo) == 0);
	REQUIRE(baseName(jpeg).find('/') == std::string::npos);
}

TEST_CASE("the header a picture goes out under is the form it was written in",
          "[screenshot]")
{
	ServingPictures serving;
	REQUIRE(serving.port > 0);
	serving.wired.screen.content = "these are its bytes";

	const testhttp::Reply jpeg = testhttp::request(serving.port, "GET",
						       "/api/v1/osd/screenshot?format=jpeg");
	REQUIRE(jpeg.transport_ok);
	REQUIRE(jpeg.code == 200);
	REQUIRE(jpeg.header("Content-Type") == "image/jpeg");
	REQUIRE(serving.wired.screen.last_format == PictureFormat::Jpeg);

	const testhttp::Reply png = testhttp::request(serving.port, "GET",
						      "/api/v1/osd/screenshot?format=png");
	REQUIRE(png.transport_ok);
	REQUIRE(png.code == 200);
	REQUIRE(png.header("Content-Type") == "image/png");
	REQUIRE(serving.wired.screen.last_format == PictureFormat::Png);

	// A request that names no form is answered as it was before the route had
	// the choice, so nothing that already reads this has to be changed. Named
	// with nothing after it is the same request: a form that submits a field
	// nobody touched sends the second and means the first.
	const char *const plainly[] = { "/api/v1/osd/screenshot",
					"/api/v1/osd/screenshot?format=" };
	for (size_t i = 0; i < sizeof(plainly) / sizeof(plainly[0]); ++i)
	{
		INFO(plainly[i]);
		serving.wired.screen.last_format = PictureFormat::Jpeg;
		const testhttp::Reply plain = testhttp::request(serving.port, "GET", plainly[i]);
		REQUIRE(plain.transport_ok);
		REQUIRE(plain.code == 200);
		REQUIRE(plain.header("Content-Type") == "image/png");
		REQUIRE(serving.wired.screen.last_format == PictureFormat::Png);
	}
}

TEST_CASE("a form the box cannot write is refused rather than ignored",
          "[screenshot]")
{
	/* The set is declared beside the route, so this is answered before the
	   handler is entered. What it must not be is a picture in whichever form
	   the reading of the name happened to fall through to, which is what a
	   free string here would have given every caller who spelt one wrong. */
	ServingPictures serving;
	REQUIRE(serving.port > 0);

	/* A form the capture can write and this route does not offer, the same
	   name in another case, the other spelling of the second one, and the list
	   itself written out as a value. A name handed over with nothing after it
	   is not here: that is no value at all rather than a wrong one, and the
	   route answers it the way it answers a request that left the name out. */
	const char *const refused[] = { "bmp", "PNG", "jpg", "png,jpeg" };
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i]);
		const unsigned before = serving.wired.screen.screen_shots;
		const testhttp::Reply r =
			testhttp::request(serving.port, "GET",
					  std::string("/api/v1/osd/screenshot?format=") + refused[i]);
		REQUIRE(r.transport_ok);
		REQUIRE(r.code == 400);
		REQUIRE(parsed(r.body)["type"].asString() == "/errors/bad-enum");
		// And the box was never asked, or a refusal would still have cost a
		// capture on the thread the box draws on.
		REQUIRE(serving.wired.screen.screen_shots == before);
	}
}

TEST_CASE("the display route offers no form and answers one", "[screenshot]")
{
	/* Whatever drives that display writes one form, so the route offers no
	   choice rather than offering one it would have to refuse, and a request
	   naming one is refused for naming a parameter the route does not
	   declare. */
	ServingPictures serving;
	REQUIRE(serving.port > 0);
	serving.wired.screen.display_status = Status::Ok;

	const testhttp::Reply r = testhttp::request(serving.port, "GET",
						    "/api/v1/osd/display/screenshot");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "image/png");

	const testhttp::Reply asked = testhttp::request(serving.port, "GET",
							"/api/v1/osd/display/screenshot?format=jpeg");
	REQUIRE(asked.transport_ok);
	REQUIRE(asked.code == 400);
}
