/*
 * test_recordings.cpp - tests for recordings
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

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/recordings.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/schema.h"

#include "jsoncpp/json/json.h"

#include <neutrinoMessages.h>

#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

/* A directory of this case file's own, with real files in it.

   Real files because one of the things this layer does is measure what is
   being written, and a case that stated the size instead of reading it would
   say nothing about the only part of that answer this layer produces. */
struct Files
{
	bool        ready;
	std::string base;

	Files() : ready(false)
	{
		char tmpl[] = "/tmp/coreapi_recordings_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		base = tmpl;
		ready = true;
	}

	~Files()
	{
		if (base.empty())
			return;
		for (size_t i = 0; i < made.size(); i++)
			unlink(made[i].c_str());
		rmdir(base.c_str());
	}

	// A file of exactly that many bytes, and its whole name.
	std::string write(const std::string &name, size_t bytes)
	{
		const std::string path = base + "/" + name;
		int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd < 0)
			return std::string();
		std::string body(bytes, 'x');
		ssize_t n = bytes ? ::write(fd, body.data(), body.size()) : 0;
		close(fd);
		if (n != (ssize_t) bytes)
			return std::string();
		made.push_back(path);
		return path;
	}

	// A name inside the directory that nothing ever wrote.
	std::string missing(const std::string &name) const { return base + "/" + name; }

	private:
		std::vector<std::string> made;

		Files(const Files &);
		Files &operator=(const Files &);
};

/* The three seams these cases drive, and nothing else: what the box is recording, and
   the socket a command goes out on. Put back from a destructor, because a case that
   fails unwinds past whatever it would have written at the end. The routes are answered
   out of the tables the server ships. */
struct Wired
{
	FakeRecordingSource     recordings;
	FakeEventSink           events;
	InstalledRecordingSource in_recordings;
	InstalledEventSink      in_events;

	Wired() : in_recordings(&recordings), in_events(&events)
	{
		httpd::setRoutesForTest(NULL);
	}

	~Wired() { httpd::setRoutesForTest(NULL); }

	private:
		Wired(const Wired &);
		Wired &operator=(const Wired &);
};

struct Reply
{
	int         code;
	std::string body;

	Reply() : code(0) {}
};

Reply send(httpd::Method m, const std::string &path,
	   httpd::AuthLevel granted = httpd::AuthLevel::System)
{
	const httpd::Response r = httpd::dispatch(m, path, "", "", "127.0.0.1", granted);
	Reply out;
	out.code = r.code;
	out.body = r.body;
	return out;
}

::Json::Value parsed(const std::string &doc)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	::Json::Value root;
	std::string errors;
	REQUIRE(reader->parse(doc.data(), doc.data() + doc.size(), &root, &errors));
	return root;
}

// The member of a list whose id is that one, so a case says which recording it
// means rather than which position it happened to land in.
::Json::Value itemWithId(const ::Json::Value &items, unsigned id)
{
	for (::Json::ArrayIndex i = 0; i < items.size(); i++)
	{
		if (items[i]["id"].asUInt() == id)
			return items[i];
	}
	return ::Json::Value();
}

} // anonymous namespace

TEST_CASE("the listing names every recording the box is taking, not one of them", "[recordings]")
{
	Files files;
	REQUIRE(files.ready);

	Wired wired;
	const std::string first = files.write("first.ts", 4096);
	const std::string second = files.write("second.ts", 12);
	const std::string third = files.write("shift.ts", 0);
	REQUIRE_FALSE(first.empty());
	REQUIRE_FALSE(second.empty());
	REQUIRE_FALSE(third.empty());

	wired.recordings.add(7, 0x11223344ULL, first, false, true);
	wired.recordings.add(9, 0x55667788ULL, second, false, false);
	wired.recordings.add(11, 0x99aabbccULL, third, true, false);

	Result<RecordingList> got = recordings::list();
	REQUIRE(got.ok());
	const RecordingList all = std::move(got).value();
	REQUIRE(all.size() == 3);

	// Named one at a time, because three entries that all read like the first
	// one would pass a case that only counted them.
	REQUIRE(all[0].id == 7);
	REQUIRE(all[0].channel_id == 0x11223344ULL);
	REQUIRE(all[0].path == first);
	REQUIRE(all[0].from_timer);
	REQUIRE_FALSE(all[0].timeshift);

	REQUIRE(all[1].id == 9);
	REQUIRE(all[1].channel_id == 0x55667788ULL);
	REQUIRE(all[1].path == second);
	REQUIRE_FALSE(all[1].from_timer);
	REQUIRE_FALSE(all[1].timeshift);

	REQUIRE(all[2].id == 11);
	REQUIRE(all[2].channel_id == 0x99aabbccULL);
	REQUIRE(all[2].timeshift);
}

TEST_CASE("what each file weighs is read off the file, and left unsaid for one that is not there", "[recordings]")
{
	Files files;
	REQUIRE(files.ready);

	Wired wired;
	const std::string written = files.write("grown.ts", 4096);
	const std::string empty = files.write("begun.ts", 0);
	REQUIRE_FALSE(written.empty());
	REQUIRE_FALSE(empty.empty());

	wired.recordings.add(1, 0x1ULL, written);
	// A recording that has just begun, which is the case a size left at nought
	// for a file nobody could reach would be indistinguishable from.
	wired.recordings.add(2, 0x2ULL, empty);
	wired.recordings.add(3, 0x3ULL, files.missing("gone.ts"));
	// A path that is a directory is not the recording it names, so it is
	// measured no more than a name nothing carries.
	wired.recordings.add(4, 0x4ULL, files.base);

	Result<RecordingList> got = recordings::list();
	REQUIRE(got.ok());
	const RecordingList all = std::move(got).value();
	REQUIRE(all.size() == 4);

	REQUIRE(all[0].size_known);
	REQUIRE(all[0].size == 4096);

	REQUIRE(all[1].size_known);
	REQUIRE(all[1].size == 0);

	REQUIRE_FALSE(all[2].size_known);
	REQUIRE(all[2].size == 0);

	REQUIRE_FALSE(all[3].size_known);
}

TEST_CASE("a box that is recording nothing answers a list and not a refusal", "[recordings]")
{
	Wired wired;

	Result<RecordingList> got = recordings::list();
	REQUIRE(got.ok());
	REQUIRE(std::move(got).value().empty());
	REQUIRE(wired.recordings.reads == 1);
}

TEST_CASE("a listing that could not be read is not a box recording nothing", "[recordings]")
{
	Wired wired;
	wired.recordings.list_status = Status::Internal;

	Result<RecordingList> got = recordings::list();
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == Status::Internal);
	REQUIRE(got.error().code == ErrorCode::RecordingListUnavailable);
}

TEST_CASE("ending one recording asks for that one and leaves the others alone", "[recordings]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");
	wired.recordings.add(5, 0x2ULL, "/tmp/b.ts");
	wired.recordings.add(6, 0x3ULL, "/tmp/c.ts");

	Result<void> done = recordings::stop(5);
	REQUIRE(done.ok());

	// One request, and for the number that was named. A case that only asked
	// whether anything was stopped would pass for a layer that stopped all
	// three or the wrong one of them.
	REQUIRE(wired.recordings.stopped.size() == 1);
	REQUIRE(wired.recordings.stopped[0] == 5);
}

TEST_CASE("ending a recording the box is not taking is refused and nothing goes out", "[recordings]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");

	Result<void> done = recordings::stop(5);
	REQUIRE_FALSE(done.ok());
	REQUIRE(done.error().status == Status::NotFound);
	REQUIRE(done.error().code == ErrorCode::NoSuchRecording);

	// The refusal is worth nothing if the request went out anyway.
	REQUIRE(wired.recordings.stopped.empty());
}

TEST_CASE("ending a recording twice is refused the second time", "[recordings]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");

	REQUIRE(recordings::stop(4).ok());

	/* What the box does after the first request is drop the recording from its
	   list, which is what the second request then reads. Nothing here keeps a
	   record of having asked: the list is the state, and a second success would
	   be this layer answering out of its own memory instead. */
	wired.recordings.recordings.clear();

	Result<void> again = recordings::stop(4);
	REQUIRE_FALSE(again.ok());
	REQUIRE(again.error().code == ErrorCode::NoSuchRecording);
	REQUIRE(wired.recordings.stopped.size() == 1);
}

TEST_CASE("a list that could not be read does not read as a recording that is not there", "[recordings]")
{
	Wired wired;
	wired.recordings.list_status = Status::Internal;

	Result<void> done = recordings::stop(4);
	REQUIRE_FALSE(done.ok());
	REQUIRE(done.error().code == ErrorCode::RecordingListUnavailable);
	REQUIRE(done.error().status == Status::Internal);
	REQUIRE(wired.recordings.stopped.empty());
}

TEST_CASE("a daemon that would not take the request is reported as that", "[recordings]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");
	wired.recordings.stop_status = Status::Internal;

	Result<void> done = recordings::stop(4);
	REQUIRE_FALSE(done.ok());
	REQUIRE(done.error().code == ErrorCode::RecordingNotStopped);
}

TEST_CASE("beginning the shift sends the command the loop reads it by", "[recordings]")
{
	Wired wired;

	Result<void> done = recordings::startTimeshift();
	REQUIRE(done.ok());
	REQUIRE(wired.events.sent.size() == 1);
	REQUIRE(wired.events.sent[0].id == (unsigned) NeutrinoMessages::EVT_START_TIMESHIFT);
	// No body: the command says what to do and names nothing to do it to.
	REQUIRE(wired.events.sent[0].body.empty());
}

TEST_CASE("beginning a shift while the box keeps one is refused", "[recordings]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");
	wired.recordings.add(5, 0x2ULL, "/tmp/shift.ts", true);

	Result<void> done = recordings::startTimeshift();
	REQUIRE_FALSE(done.ok());
	REQUIRE(done.error().status == Status::Conflict);
	REQUIRE(done.error().code == ErrorCode::TimeshiftRunning);
	REQUIRE(wired.events.sent.empty());
}

TEST_CASE("a recording that is not the shift does not stand in for one", "[recordings]")
{
	Wired wired;
	// Two ordinary recordings, so a layer that answered on the size of the
	// list rather than on what is in it would refuse this.
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");
	wired.recordings.add(5, 0x2ULL, "/tmp/b.ts");

	REQUIRE(recordings::startTimeshift().ok());
	REQUIRE(wired.events.sent.size() == 1);
}

TEST_CASE("ending the shift sends its own command, and is refused while there is none", "[recordings]")
{
	Wired wired;

	Result<void> none = recordings::stopTimeshift();
	REQUIRE_FALSE(none.ok());
	REQUIRE(none.error().status == Status::NotFound);
	REQUIRE(none.error().code == ErrorCode::NoTimeshift);
	REQUIRE(wired.events.sent.empty());

	wired.recordings.add(5, 0x2ULL, "/tmp/shift.ts", true);

	Result<void> done = recordings::stopTimeshift();
	REQUIRE(done.ok());
	REQUIRE(wired.events.sent.size() == 1);
	REQUIRE(wired.events.sent[0].id == (unsigned) NeutrinoMessages::EVT_STOP_TIMESHIFT);
	// Not the same command as beginning one, which is the whole of what
	// separates the two routes.
	REQUIRE(wired.events.sent[0].id != (unsigned) NeutrinoMessages::EVT_START_TIMESHIFT);
}

TEST_CASE("a loop that did not take the command is reported as that", "[recordings]")
{
	Wired wired;
	wired.events.answer = Status::NotSupported;

	Result<void> done = recordings::startTimeshift();
	REQUIRE_FALSE(done.ok());
	REQUIRE(done.error().code == ErrorCode::CommandNotSent);
}

TEST_CASE("the route answers every recording, each with its own members", "[recordings][httpd]")
{
	Files files;
	REQUIRE(files.ready);

	Wired wired;
	const std::string one = files.write("one.ts", 1024);
	REQUIRE_FALSE(one.empty());

	wired.recordings.add(7, 0x112233445566ULL, one, false, true);
	wired.recordings.add(9, 0x778899aabbccULL, files.missing("two.ts"), true, false);

	const Reply r = send(httpd::Get, "/api/v1/recordings", httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["items"].isArray());
	REQUIRE(doc["items"].size() == 2);

	const ::Json::Value first = itemWithId(doc["items"], 7);
	REQUIRE(first.isObject());
	REQUIRE(first["channel_id"].asString() == "112233445566");
	REQUIRE(first["path"].asString() == one);
	REQUIRE(first["size"].asUInt64() == 1024);
	REQUIRE(first["timeshift"].asBool() == false);
	REQUIRE(first["started_by"].asString() == "timer");

	const ::Json::Value second = itemWithId(doc["items"], 9);
	REQUIRE(second.isObject());
	REQUIRE(second["channel_id"].asString() == "778899aabbcc");
	REQUIRE(second["timeshift"].asBool() == true);
	REQUIRE(second["started_by"].asString() == "immediate");
	// The one member that is absent rather than nought, and this is the answer
	// that says which of the two it is.
	REQUIRE_FALSE(second.isMember("size"));
}

TEST_CASE("the route that ends one takes the number the listing gave", "[recordings][httpd]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");
	wired.recordings.add(5, 0x2ULL, "/tmp/b.ts");

	const Reply r = send(httpd::Delete, "/api/v1/recordings/5");
	// Accepted and not done: what stops the writing answers nobody.
	REQUIRE(r.code == 202);
	REQUIRE(wired.recordings.stopped.size() == 1);
	REQUIRE(wired.recordings.stopped[0] == 5);
}

TEST_CASE("ending a recording that is not there answers that, rather than succeeding quietly", "[recordings][httpd]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");

	const Reply r = send(httpd::Delete, "/api/v1/recordings/5");
	REQUIRE(r.code == 404);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["type"].asString() == "/errors/no-such-recording");
	REQUIRE(wired.recordings.stopped.empty());
}

TEST_CASE("the shift is reached by its own name and not as a number", "[recordings][httpd]")
{
	Wired wired;
	wired.recordings.add(5, 0x2ULL, "/tmp/shift.ts", true);

	/* Both of these paths match the route that binds a number in that
	   position. What settles it is that this one writes the segment out, and
	   a request that reached the other would be a request to end recording
	   number nothing. */
	const Reply ended = send(httpd::Delete, "/api/v1/recordings/timeshift");
	REQUIRE(ended.code == 202);
	REQUIRE(wired.recordings.stopped.empty());
	REQUIRE(wired.events.sent.size() == 1);
	REQUIRE(wired.events.sent[0].id == (unsigned) NeutrinoMessages::EVT_STOP_TIMESHIFT);

	const Reply begun = send(httpd::Post, "/api/v1/recordings/timeshift");
	REQUIRE(begun.code == 409);
	const ::Json::Value doc = parsed(begun.body);
	REQUIRE(doc["type"].asString() == "/errors/timeshift-running");
}

TEST_CASE("beginning the shift over the wire reaches the loop", "[recordings][httpd]")
{
	Wired wired;

	const Reply r = send(httpd::Post, "/api/v1/recordings/timeshift");
	REQUIRE(r.code == 202);
	REQUIRE(wired.events.sent.size() == 1);
	REQUIRE(wired.events.sent[0].id == (unsigned) NeutrinoMessages::EVT_START_TIMESHIFT);
}

TEST_CASE("a caller granted no more than a read cannot end a recording", "[recordings][httpd]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");

	const Reply r = send(httpd::Delete, "/api/v1/recordings/4", httpd::AuthLevel::Read);
	REQUIRE(r.code == 403);
	REQUIRE(wired.recordings.stopped.empty());
}

TEST_CASE("a number no recording could carry is turned down before the box is asked", "[recordings][httpd]")
{
	Wired wired;
	wired.recordings.add(4, 0x1ULL, "/tmp/a.ts");

	// Nought is what a recording carries while the daemon has not numbered it,
	// and there is nothing to ask the daemon to stop under a number it never
	// gave.
	const Reply zero = send(httpd::Delete, "/api/v1/recordings/0");
	REQUIRE(zero.code == 400);

	// Above what the daemon's own counter reaches, which is the bound that
	// keeps a value from arriving as a different number where the accessor is
	// wider than the id.
	const Reply wide = send(httpd::Delete, "/api/v1/recordings/4294967297");
	REQUIRE(wide.code == 400);

	REQUIRE(wired.recordings.stopped.empty());
}
