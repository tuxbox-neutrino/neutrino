/*
 * test_captureguard.cpp - no picture of the screen while it shows a password
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * captureguard is what the message with the first password holds and what
 * every capture of the screen asks before it reads and before it writes.
 * These cases drive the shipped object. A capture here is the order the
 * screenshot code follows: admitted or nothing is read, clean or nothing
 * read is written. That code reads a video decoder and cannot run here; a
 * scan (check-capture-exclusion.sh) holds it to this order.
 */

#include "support/catch.hpp"

#include <driver/captureguard.h>
#include <coreapi/base/accessfiles.h>

#include "helper.h"
#include "yhook.h"
#include "yprotect.h"
#include "ystatic.h"
#include "yhttpd_mods/mod_sendfile.h"

#include <configfile.h>

#include <atomic>
#include <thread>
#include <cstdio>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace
{

// What one capture did: read the screen, and wrote what it read.
struct Taken
{
	bool read;
	bool written;
};

// A capture whose reading is done by the function given, so that a case can
// put something on the screen while the capture is reading it.
template <typename Reading>
Taken capture(Reading reading)
{
	Taken t = { false, false };
	captureguard::Capture c;
	if (!c.admitted())
		return t;
	t.read = true;
	reading();
	if (!c.clean())
		return t;
	t.written = true;
	return t;
}

void nothing() {}

void restFor(int ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long) (ms % 1000) * 1000 * 1000;
	nanosleep(&ts, NULL);
}

/* A ceiling for the threaded case and not a wait for a signal, so that a guard
   that blocked one side on the other fails the case instead of hanging it. */
const int kWaitMs = 3000;

bool waitFor(const std::atomic<bool> &flag)
{
	for (int waited = 0; waited < kWaitMs && !flag; waited += 5)
		restFor(5);
	return flag;
}

// A capture reading on a thread of its own until it is let go.
struct ReadingElsewhere
{
	std::atomic<bool> reading;
	std::atomic<bool> let_go;
	Taken taken;
	pthread_t thread;
	bool started;

	ReadingElsewhere() : reading(false), let_go(false), thread(), started(false)
	{
		taken.read = false;
		taken.written = false;
		started = pthread_create(&thread, NULL, &run, this) == 0;
	}

	~ReadingElsewhere()
	{
		let_go = true;
		if (started)
			pthread_join(thread, NULL);
	}

	void join()
	{
		let_go = true;
		if (started)
			pthread_join(thread, NULL);
		started = false;
	}

	static void *run(void *arg)
	{
		ReadingElsewhere *r = static_cast<ReadingElsewhere *>(arg);
		struct Hold
		{
			ReadingElsewhere *r;
			void operator()() const
			{
				r->reading = true;
				waitFor(r->let_go);
			}
		} hold = { r };
		r->taken = capture(hold);
		return NULL;
	}

	private:
		ReadingElsewhere(const ReadingElsewhere &);
		ReadingElsewhere &operator=(const ReadingElsewhere &);
};

} // namespace

TEST_CASE("G1: with no password on the screen a capture reads and writes", "[driver][captureguard]")
{
	captureguard::Capture c;
	REQUIRE(c.admitted());
	REQUIRE(c.clean());

	const Taken t = capture(nothing);
	REQUIRE(t.read);
	REQUIRE(t.written);
}

TEST_CASE("G2: while a password is on the screen nothing is read or written, and afterwards captures work again", "[driver][captureguard]")
{
	{
		captureguard::Exclusion shown;
		captureguard::Capture c;
		REQUIRE_FALSE(c.admitted());
		REQUIRE_FALSE(c.clean());

		const Taken t = capture(nothing);
		REQUIRE_FALSE(t.read);
		REQUIRE_FALSE(t.written);
	}

	captureguard::Capture after;
	REQUIRE(after.admitted());
	REQUIRE(after.clean());
	const Taken t = capture(nothing);
	REQUIRE(t.read);
	REQUIRE(t.written);
}

/* The message came up while the capture was reading and went again before it
   had finished: what it read may hold the password, so nothing is written,
   though nothing stands any more when it asks. */
TEST_CASE("G3: a capture reading when the password comes on writes nothing, even once it is gone", "[driver][captureguard]")
{
	struct ShownMeanwhile
	{
		void operator()() const { captureguard::Exclusion shown; }
	} meanwhile;

	const Taken t = capture(meanwhile);
	REQUIRE(t.read);
	REQUIRE_FALSE(t.written);

	// The next one is a capture like any other.
	const Taken next = capture(nothing);
	REQUIRE(next.read);
	REQUIRE(next.written);
}

TEST_CASE("G4: a password shown and gone before a capture began leaves it alone", "[driver][captureguard]")
{
	{
		captureguard::Exclusion shown;
	}
	captureguard::Capture c;
	REQUIRE(c.admitted());
	REQUIRE(c.clean());
}

/* Scopes and not new and delete, so that a check that fails here unwinds both
   exclusions and leaves the cases after it alone. */
TEST_CASE("G5: two messages at once keep captures out until the last is gone", "[driver][captureguard]")
{
	{
		captureguard::Exclusion first;
		{
			captureguard::Exclusion second;
			REQUIRE_FALSE(captureguard::Capture().admitted());
		}
		REQUIRE_FALSE(captureguard::Capture().admitted());
		REQUIRE_FALSE(capture(nothing).read);
	}
	REQUIRE(captureguard::Capture().admitted());
	REQUIRE(capture(nothing).written);
}

/* The two sides on two threads, as on the box: the message is drawn by the
   main loop while a web worker is reading the screen. Neither waits for the
   other, and the capture keeps nothing. */
TEST_CASE("G6: a password put on the screen from another thread spoils a capture reading at the time", "[driver][captureguard]")
{
	ReadingElsewhere elsewhere;
	REQUIRE(elsewhere.started);
	REQUIRE(waitFor(elsewhere.reading));
	{
		captureguard::Exclusion shown;
	}
	elsewhere.join();
	REQUIRE(elsewhere.taken.read);
	REQUIRE_FALSE(elsewhere.taken.written);
}

/* Where a picture waits for its name: beside it, short whatever the name's
   length, and never the same for two captures. */
TEST_CASE("G7: a picture is written beside its name under a short name of its own", "[driver][captureguard]")
{
	const std::string longest = "/media/hdd/screenshots/" + std::string(255, 'x');
	const std::string first = captureguard::sideName(longest);
	const std::string second = captureguard::sideName(longest);
	REQUIRE(first.compare(0, 23, "/media/hdd/screenshots/") == 0);
	REQUIRE(first.find('/', 23) == std::string::npos);
	REQUIRE(first.size() - 23 <= 64u);
	REQUIRE(first != second);
	REQUIRE(first != longest);

	// A bare name stays where it is, and a name at the top of the tree too.
	REQUIRE(captureguard::sideName("picture.png").find('/') == std::string::npos);
	REQUIRE(captureguard::sideName("/picture.png").rfind('/') == 0);
}

namespace
{

// The entries of dir, by name.
std::vector<std::string> entries(const std::string &dir)
{
	std::vector<std::string> out;
	DIR *d = opendir(dir.c_str());
	if (d == NULL)
		return out;
	for (struct dirent *e; (e = readdir(d)) != NULL; )
	{
		const std::string name = e->d_name;
		if (name != "." && name != "..")
			out.push_back(name);
	}
	closedir(d);
	return out;
}

std::string contentOf(const std::string &path)
{
	std::string out;
	FILE *f = fopen(path.c_str(), "rb");
	if (f == NULL)
		return "(no file)";
	char buf[256];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	fclose(f);
	return out;
}

bool writeText(const std::string &path, const std::string &text)
{
	FILE *f = fopen(path.c_str(), "w");
	if (f == NULL)
		return false;
	const bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
	return fclose(f) == 0 && ok;
}

} // namespace

/* What a capture writes before captureguard agreed to it is reachable by no
   name: not in the directory it is published to, and not anywhere the file
   was made. Only publish() gives it one, and a Staging dropped unpublished
   leaves nothing. */
TEST_CASE("G8: a picture waiting for captureguard is in no directory a reader can list", "[driver][captureguard]")
{
	char dir[] = "/tmp/captureguard-test-XXXXXX";
	REQUIRE(mkdtemp(dir) != NULL);
	const std::string target = std::string(dir) + "/screenshot.png";

	{
		captureguard::Staging staging;
		REQUIRE(staging.ok());
		REQUIRE_FALSE(staging.written());
		// written as a program the capture starts writes it, by the name given
		REQUIRE(writeText(staging.path(), "the password"));
		REQUIRE(staging.written());

		// the file has no name: a second reader finds it nowhere
		struct stat st;
		REQUIRE(stat(staging.path().c_str(), &st) == 0);
		REQUIRE(st.st_nlink == 0u);
		REQUIRE(entries(dir).empty());
	}
	// dropped unpublished: still nothing
	REQUIRE(entries(dir).empty());

	{
		captureguard::Staging staging;
		REQUIRE(writeText(staging.path(), "a picture"));
		REQUIRE(staging.publish(target));
		REQUIRE(contentOf(target) == "a picture");
		// and nothing beside it
		REQUIRE(entries(dir) == std::vector<std::string>(1, "screenshot.png"));
		// a second picture takes its place whole
		captureguard::Staging next;
		REQUIRE(writeText(next.path(), "the next picture"));
		REQUIRE(next.publish(target));
		REQUIRE(contentOf(target) == "the next picture");
		REQUIRE(entries(dir).size() == 1u);
	}

	unlink(target.c_str());
	rmdir(dir);
}

namespace
{

// A lock the producer and the consumer below share, as the GraphLCD driver's
// own is shared between its mirror and dumpBuffer.
struct TestLock
{
	pthread_mutex_t m;
	TestLock() { pthread_mutex_init(&m, NULL); }
	~TestLock() { pthread_mutex_destroy(&m); }
	void lock() { pthread_mutex_lock(&m); }
	void unlock() { pthread_mutex_unlock(&m); }
};

// What the mirror and its readers share: the lock and the picture.
struct Mirror
{
	TestLock lock;
	std::string bitmap;
	std::atomic<bool> drawing;
	std::atomic<bool> go_on;
	std::atomic<bool> shown_now;
	std::atomic<bool> drawn;

	Mirror() : drawing(false), go_on(false), shown_now(false), drawn(false) {}
};

/* The mirror's drawing: it was admitted, reads the screen after the password
   came on, and pauses before captureguard is asked again. */
void *produce(void *arg)
{
	Mirror *m = static_cast<Mirror *>(arg);
	struct Draw
	{
		Mirror *m;
		bool operator()() const
		{
			m->drawing = true;
			waitFor(m->shown_now);
			m->bitmap = "the password";
			m->drawn = true;
			waitFor(m->go_on);
			return true;
		}
	} draw = { m };
	struct Clear
	{
		Mirror *m;
		void operator()() const { m->bitmap.clear(); }
	} clear = { m };
	captureguard::drawShared(m->lock, draw, clear);
	return NULL;
}

// A picture of the display taken meanwhile, as the routes take it: its own
// Capture, the bitmap read under the lock, and published where it is clean.
struct Consumer
{
	Mirror *m;
	std::string published;
	std::atomic<bool> done;
	pthread_t thread;

	explicit Consumer(Mirror *mirror) : m(mirror), done(false), thread() {}

	static void *run(void *arg)
	{
		Consumer *c = static_cast<Consumer *>(arg);
		captureguard::Capture capture;
		std::string read;
		if (capture.admitted())
		{
			c->m->lock.lock();
			read = c->m->bitmap;
			c->m->lock.unlock();
		}
		if (capture.clean())
			c->published = read;
		c->done = true;
		return NULL;
	}
};

} // namespace

/* A drawing the password came into, paused before it is checked, the message
   gone again, and only then a picture of the display asked for: that picture
   is admitted and clean by its own Capture, so only the lock keeps it from
   publishing the drawing before the mirror has cleared it. */
TEST_CASE("G9: a picture of the mirror never holds a drawing that was not checked yet", "[driver][captureguard]")
{
	Mirror m;
	pthread_t producer;
	REQUIRE(pthread_create(&producer, NULL, &produce, &m) == 0);
	REQUIRE(waitFor(m.drawing));
	{
		captureguard::Exclusion shown;
		m.shown_now = true;
		// the drawing holds the password now
		REQUIRE(waitFor(m.drawn));
	}

	Consumer c(&m);
	REQUIRE(pthread_create(&c.thread, NULL, &Consumer::run, &c) == 0);
	// the picture has as long as it needs to come out, unless something holds it back
	for (int waited = 0; waited < 300 && !c.done; waited += 5)
		restFor(5);
	m.go_on = true;
	pthread_join(producer, NULL);
	pthread_join(c.thread, NULL);

	REQUIRE(c.done);
	REQUIRE(c.published.find("password") == std::string::npos);
	REQUIRE(m.bitmap.empty());
}

/* The file a capture writes before captureguard agreed has no name, but this
   process can still open it through /proc, as a program the capture starts
   does. What the web server opens that way goes out through none of its
   gates, whatever its folders are: a file without a name is never sent. */
TEST_CASE("G10: a picture waiting for captureguard goes out through no gate of the web server", "[driver][captureguard]")
{
	captureguard::Staging staging;
	REQUIRE(staging.ok());
	REQUIRE(writeText(staging.path(), "the password"));
	// the writer still reaches it
	REQUIRE(contentOf(staging.path()) == "the password");

	const int fd = open(staging.path().c_str(), O_RDONLY | O_CLOEXEC);
	REQUIRE(fd >= 0);
	CHECK(yprotect::isProtected(fd));
	close(fd);
	struct stat st;
	CHECK(ystatic::openToSend(staging.path(), st) < 0);
}

namespace
{

// the .capture- names in /tmp
size_t captureNames()
{
	size_t n = 0;
	const std::vector<std::string> all = entries("/tmp");
	for (size_t i = 0; i < all.size(); i++)
		if (all[i].compare(0, 9, ".capture-") == 0)
			n++;
	return n;
}

// What a reader made of the staging file by the name it was made under, while
// the Staging is being made.
struct Reading
{
	std::string name;
	std::atomic<bool> done;
	std::atomic<bool> asked;
	std::atomic<bool> opened;
	bool refused;
	bool checked;
	bool done_in_window;
	dev_t dev;
	ino_t ino;
	std::thread thread;
	Reading() : done(false), asked(false), opened(false), refused(false), checked(false), done_in_window(true),
	            dev(0), ino(0) {}
};
Reading *reading = NULL;

// the descriptors this process holds on the file of that device and inode
size_t descriptorsOn(dev_t dev, ino_t ino)
{
	size_t n = 0;
	DIR *d = opendir("/proc/self/fd");
	if (d == NULL)
		return 0;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		struct stat st;
		if (e->d_name[0] != '.' && fstat(atoi(e->d_name), &st) == 0 && st.st_dev == dev && st.st_ino == ino)
			n++;
	}
	closedir(d);
	return n;
}

/* A gate has come as far as its question. The sender opened the staging file
   itself where a second descriptor on it stands beside the Staging's own. */
void askedAboutStaging()
{
	Reading *r = reading;
	if (r == NULL)
		return;
	if (r->ino != 0 && descriptorsOn(r->dev, r->ino) >= 2)
		r->opened = true;
	r->asked = true;
}

// until the reader asks, which it does at once, and the moment after, in which a
// reader that is let through would be done
void awaitQuestion(Reading *r)
{
	for (int waited = 0; waited < 10000 && !r->asked && !r->done; waited++)
		restFor(1);
	for (int waited = 0; waited < 300 && !r->done; waited += 5)
		restFor(5);
	r->done_in_window = r->done;
}
size_t names_at_made = 0;
bool made_unnamed = false;

void countAtMade(const std::string &name)
{
	made_unnamed = name.empty();
	names_at_made = captureNames();
}

// a reader asks about a descriptor opened by the name, as every gate asks
void askWhileMade(const std::string &name)
{
	Reading *r = reading;
	r->name = name;
	coreapi::accessfiles::setAskingForTest(askedAboutStaging);
	r->thread = std::thread([r]() {
		const int fd = open(r->name.c_str(), O_RDONLY | O_CLOEXEC);
		r->opened = fd >= 0;
		if (fd >= 0) {
			r->refused = yprotect::isProtected(fd);
			close(fd);
		}
		r->done = true;
	});
	awaitQuestion(r);
}

void sawChecked(int)
{
	if (reading != NULL)
		reading->checked = true;
}

// the sender asked for the staging file through an override folder of /tmp,
// everything sent
void sendWhileMade(const std::string &name)
{
	Reading *r = reading;
	r->name = name;
	struct stat st;
	if (stat(name.c_str(), &st) == 0) {
		r->dev = st.st_dev;
		r->ino = st.st_ino;
	}
	coreapi::accessfiles::setAskingForTest(askedAboutStaging);
	r->thread = std::thread([r]() {
		CmodSendfile sendfile;
		CConfigFile config(',');
		CStringList list;
		sendfile.Hook_ReadConfig(&config, list);
		CyhookHandler hh;
		hh.WebserverConfigList["WebsiteMain.directory"] = "/nonexistent-web";
		hh.WebserverConfigList["WebsiteMain.override_directory"] = "/tmp";
		hh.WebserverConfigList["mod_sendfile.sendAll"] = "true";
		hh.Method = M_GET;
		const std::string base = r->name.substr(r->name.rfind('/') + 1);
		hh.UrlData["url"] = "/" + base;
		hh.UrlData["path"] = "/";
		hh.UrlData["filename"] = base;
		hh.UrlData["fileext"] = "";
		sendfile.Hook_PrepareResponse(&hh);
		r->refused = !(hh.status == HANDLED_SENDFILE && hh.SendFileFd >= 0);
		hh.dropSendFileFd();
		r->done = true;
	});
	awaitQuestion(r);
}

struct MadeBack
{
	~MadeBack()
	{
		captureguard::setStagingMadeForTest(NULL, false);
		CmodSendfile::setCheckedForTest(NULL);
		coreapi::accessfiles::setAskingForTest(NULL);
		reading = NULL;
	}
};

} // namespace

/* Where /tmp takes O_TMPFILE the file never has a name: at the moment it is made
   nothing new stands in /tmp. */
TEST_CASE("G11: a picture waiting for captureguard is made without a name where /tmp can", "[driver][captureguard]")
{
	MadeBack back;
	const size_t before = captureNames();
	captureguard::setStagingMadeForTest(countAtMade, false);
	captureguard::Staging staging;
	REQUIRE(staging.ok());
	// /tmp is tmpfs here, which takes O_TMPFILE
	REQUIRE(made_unnamed);
	CHECK(names_at_made == before);
	struct stat st;
	REQUIRE(stat(staging.path().c_str(), &st) == 0);
	CHECK(st.st_nlink == 0u);
}

/* Made under a name, the name stands for a moment, and a reader can open the
   file by it. That reader is asked about it only once the name is gone, and is
   refused for that. */
TEST_CASE("G12: a picture made under a name for a moment is refused to a reader who opened it then", "[driver][captureguard]")
{
	MadeBack back;
	Reading r;
	reading = &r;
	captureguard::setStagingMadeForTest(askWhileMade, true);
	{
		captureguard::Staging staging;
		REQUIRE(staging.ok());
	}
	if (r.thread.joinable())
		r.thread.join();
	coreapi::accessfiles::setAskingForTest(NULL);
	REQUIRE(r.opened);
	CHECK(r.asked);
	// held back while the name stood, and refused once it was gone
	CHECK_FALSE(r.done_in_window);
	CHECK(r.refused);
}

/* The schedule from the review: a sender lists /tmp through an override folder,
   opens the file by its name while it stands, and would read its length later.
   It never gets as far as that. */
TEST_CASE("G13: the sender never reaches the length of a picture made under a name for a moment", "[driver][captureguard]")
{
	MadeBack back;
	Reading r;
	reading = &r;
	CmodSendfile::setCheckedForTest(sawChecked);
	captureguard::setStagingMadeForTest(sendWhileMade, true);
	{
		captureguard::Staging staging;
		REQUIRE(staging.ok());
		REQUIRE(writeText(staging.path(), "the password"));
	}
	if (r.thread.joinable())
		r.thread.join();
	coreapi::accessfiles::setAskingForTest(NULL);
	// the sender opened the staging file itself and asked about it while the name stood
	REQUIRE(r.opened);
	CHECK(r.asked);
	CHECK_FALSE(r.done_in_window);
	CHECK_FALSE(r.checked);
	CHECK(r.refused);
}

