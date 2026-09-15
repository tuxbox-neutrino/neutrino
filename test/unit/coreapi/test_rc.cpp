/*
 * test_rc.cpp - tests for remote keys
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
#include "support/rcfixture.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/osd.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include "jsoncpp/json/json.h"

#include <neutrinoMessages.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

/* The generated table itself, and every figure below is taken off it. A count written
   out here would say only that somebody typed one number twice; read from the table it
   falls the day a key leaves the input layer and rises the day one is added.

   The kernel's header first, for the reason the layer under test includes it first: the
   generated one only fills in the names that one does not carry. */
#include <linux/input.h>
#include <src/tools/rcsim.h>

/* The application owns both of these and this binary holds neither. The
   pointer is defined by the case that drives the message queue; the lock is a
   flag held by a screen no test links, and false is what a build holding no
   such flag answers. Nothing here reads the lock through the device below. */
extern CRCInput *g_RCInput;
bool coreapi::remoteControlLocked() { return false; }

using namespace coreapi;

namespace
{

const size_t kGeneratedKeyCount = sizeof(keyname) / sizeof(keyname[0]);

/* Answered out of the tables the server ships rather than out of one a case
   installed, so these read the same list a request off a socket is answered
   from. Cleared on the way in as well as on the way out, because a case
   elsewhere that failed and unwound past its own restore would otherwise
   decide what these see. */
struct ShippedRoutes
{
	ShippedRoutes() { httpd::setRoutesForTest(NULL); }
	~ShippedRoutes() { httpd::setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

struct Reply
{
	int         code;
	std::string body;

	Reply() : code(0) {}
};

Reply call(httpd::Method m, const std::string &path, const std::string &body,
	   httpd::AuthLevel as)
{
	const httpd::Response r = httpd::dispatch(m, path, "", body, "127.0.0.1", as);

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
	std::string errs;
	REQUIRE(reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs));
	return root;
}

} // namespace

TEST_CASE("a key name reaches the device as its code", "[rc]")
{
	FakeInputDevice device;
	InstalledInputDevice installed(&device);

	REQUIRE(osd::sendKey("KEY_0").ok());
	REQUIRE(device.writes == 1);
	REQUIRE(device.last_code == (unsigned long) KEY_0);
}

/* The one name the table stands against two codes, and the row written first
   is the one a press has always reached. Without this the walk could answer
   either and the case above would not know. */
TEST_CASE("a name the table carries twice reaches the first of the two", "[rc]")
{
	FakeInputDevice device;
	InstalledInputDevice installed(&device);

	REQUIRE(osd::sendKey("KEY_HOME").ok());
	REQUIRE(device.last_code == (unsigned long) KEY_HOME);
	// The second row of that name, so a walk that answered the last match
	// rather than the first fails here and says which it answered.
	REQUIRE(device.last_code != (unsigned long) KEY_EXIT);
}

TEST_CASE("a name nobody has is refused and nothing is sent", "[rc]")
{
	FakeInputDevice device;
	InstalledInputDevice installed(&device);

	// The counter proves it moves before it is asked to have stayed still. A
	// case that only asked for the second would pass against a device nothing
	// ever reaches.
	REQUIRE(osd::sendKey("KEY_0").ok());
	const unsigned before = device.writes;
	REQUIRE(before == 1);

	Result<void> r = osd::sendKey("KEY_GIBTESNICHT");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::NoSuchKey);
	REQUIRE(device.writes == before);
}

/* A name longer than the ceiling is turned away on its length rather than
   walked against every row, and the answer is the one a name nobody has gets:
   it is not a key of this box. */
TEST_CASE("a name past the ceiling is refused and nothing is sent", "[rc]")
{
	FakeInputDevice device;
	InstalledInputDevice installed(&device);

	const std::string huge(osd::MAX_KEY_NAME_BYTES + 1, 'K');
	Result<void> r = osd::sendKey(huge);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::NoSuchKey);
	REQUIRE(device.writes == 0);
}

// The ceiling against the table it is a ceiling for, so a longer name added to
// the input layer is a failing case here rather than a key nobody can send.
TEST_CASE("every name the box knows fits the ceiling", "[rc]")
{
	REQUIRE(kGeneratedKeyCount > 0);
	for (size_t i = 0; i < kGeneratedKeyCount; i++)
	{
		INFO(keyname[i].name);
		REQUIRE(std::strlen(keyname[i].name) <= osd::MAX_KEY_NAME_BYTES);
	}
}

/* Not a transcription: the count and every name come from the generated table,
   and a list written out by hand here would drift the day a key is added.
   Compared name by name rather than only counted, because two lists of one
   length can still be two lists. */
TEST_CASE("the name list is the generated one", "[rc]")
{
	const KeyNameList names = osd::keyNames().value();
	REQUIRE(names.size() == kGeneratedKeyCount);
	for (size_t i = 0; i < kGeneratedKeyCount; i++)
	{
		INFO(i << ": " << keyname[i].name);
		REQUIRE(names[i] == keyname[i].name);
	}
}

/* The node turning a key away is the one failure this transport can report,
   and the key did not go out. The layer has to pass that on rather than
   smoothing it into an ok, or a caller is told the box moved when it did not.
   */
TEST_CASE("a key the node would not take is reported and not swallowed", "[rc]")
{
	FakeInputDevice device;
	device.send_status = Status::Denied;
	InstalledInputDevice installed(&device);

	Result<void> r = osd::sendKey("KEY_0");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Denied);
	REQUIRE(r.error().code == ErrorCode::KeyNotSent);
	// It did reach the device, which is what tells this apart from a name that
	// was turned away above it.
	REQUIRE(device.writes == 1);
}

TEST_CASE("the lock reads what the box holds", "[rc]")
{
	FakeInputDevice device;
	InstalledInputDevice installed(&device);

	REQUIRE(osd::locked().ok());
	REQUIRE_FALSE(osd::locked().value());

	device.is_locked = true;
	REQUIRE(osd::locked().value());
}

TEST_CASE("a lock nothing holds is reported and not read as unlocked", "[rc]")
{
	FakeInputDevice device;
	device.lock_status = Status::NotSupported;
	device.is_locked = true;
	InstalledInputDevice installed(&device);

	Result<bool> r = osd::locked();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotSupported);
	REQUIRE(r.error().code == ErrorCode::RemoteLockUnreadable);
}

/* Two commands and not one carrying a flag, because what reads them off the
   loop reads the number and nothing else: one message locks and another
   unlocks, and a single number with a body would be a message nothing there
   knows. */
TEST_CASE("the two directions of the lock are two commands", "[rc]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(osd::setLocked(true).ok());
	REQUIRE(osd::setLocked(false).ok());

	REQUIRE(sink.sent.size() == 2);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::LOCK_RC);
	REQUIRE(sink.sent[1].id == (unsigned) NeutrinoMessages::UNLOCK_RC);
	// Neither carries anything: what reads them reads the whole command out of
	// the number, and a body would be a block nothing there frees.
	REQUIRE(sink.sent[0].body.empty());
	REQUIRE(sink.sent[1].body.empty());
}

TEST_CASE("a lock nobody took is reported and not answered as done", "[rc]")
{
	FakeEventSink sink;
	sink.unsupported = NeutrinoMessages::LOCK_RC;
	InstalledEventSink installed(&sink);

	Result<void> r = osd::setLocked(true);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotSupported);
	REQUIRE(sink.sent.empty());

	// Only the one named message is refused, or this would pass against a sink
	// that refused everything.
	REQUIRE(osd::setLocked(false).ok());
	REQUIRE(sink.sent.size() == 1);
}

TEST_CASE("the key route sends what it was handed", "[rc]")
{
	ShippedRoutes shipped;
	InstalledDependencies wired;

	const Reply r = call(httpd::Post, "/api/v1/osd/remote/key",
			     "{\"name\":\"KEY_0\"}", httpd::AuthLevel::Write);
	REQUIRE(r.code == httpd::StatusAccepted);
	REQUIRE(wired.input.writes == 1);
	REQUIRE(wired.input.last_code == (unsigned long) KEY_0);
}

TEST_CASE("the key route turns away a name the box does not have", "[rc]")
{
	ShippedRoutes shipped;
	InstalledDependencies wired;

	const Reply r = call(httpd::Post, "/api/v1/osd/remote/key",
			     "{\"name\":\"KEY_GIBTESNICHT\"}", httpd::AuthLevel::Write);
	REQUIRE(r.code == httpd::StatusBadRequest);
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/no-such-key");
	REQUIRE(wired.input.writes == 0);
}

TEST_CASE("the list route answers every name the box knows", "[rc]")
{
	ShippedRoutes shipped;
	InstalledDependencies wired;

	const Reply r = call(httpd::Get, "/api/v1/osd/remote/keys", "", httpd::AuthLevel::Read);
	REQUIRE(r.code == httpd::StatusOk);

	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == kGeneratedKeyCount);
	for (::Json::ArrayIndex i = 0; i < items.size(); i++)
	{
		INFO(i << ": " << keyname[i].name);
		REQUIRE(items[i].asString() == keyname[i].name);
	}
}

TEST_CASE("the lock route answers the flag and asks for both directions", "[rc]")
{
	ShippedRoutes shipped;
	InstalledDependencies wired;
	wired.input.is_locked = true;

	const Reply read = call(httpd::Get, "/api/v1/osd/remote", "", httpd::AuthLevel::Read);
	REQUIRE(read.code == httpd::StatusOk);
	REQUIRE(parsed(read.body)["locked"].asBool());

	const Reply off = call(httpd::Put, "/api/v1/osd/remote",
			       "{\"locked\":false}", httpd::AuthLevel::Write);
	REQUIRE(off.code == httpd::StatusAccepted);
	REQUIRE(wired.events.sent.size() == 1);
	REQUIRE(wired.events.sent[0].id == (unsigned) NeutrinoMessages::UNLOCK_RC);

	const Reply on = call(httpd::Put, "/api/v1/osd/remote",
			      "{\"locked\":true}", httpd::AuthLevel::Write);
	REQUIRE(on.code == httpd::StatusAccepted);
	REQUIRE(wired.events.sent.size() == 2);
	REQUIRE(wired.events.sent[1].id == (unsigned) NeutrinoMessages::LOCK_RC);
}

namespace
{

/* The node the shipped build writes a key to, read off the list itself rather
   than written out again here: a second copy of the path is the very thing
   this file is about. Held for the life of the program, because what moves it
   below hands out a pointer and keeps it. */
const char *shippedKeyNode()
{
	static const std::string held = injectedKeyNodes().back();
	return held.c_str();
}

/* Moves that node off the one a box uses, so that a run of this does not put
   keys into a box running beside it. A fifo and not a file: the loop selects
   on what it opened, and a file is always readable, so a file would have it
   spinning over bytes nobody wrote. */
struct InjectedNode
{
	std::string path;

	InjectedNode() : path(std::string("/tmp/coreapi-key.") + std::to_string(getpid()))
	{
		// Read before it is moved, and not empty, or the way back is a path
		// the next case would write keys to.
		REQUIRE_FALSE(std::string(shippedKeyNode()).empty());
		unlink(path.c_str());
		REQUIRE(mkfifo(path.c_str(), 0600) == 0);
		setInjectedKeyNodeForTest(path.c_str());
	}

	~InjectedNode()
	{
		setInjectedKeyNodeForTest(shippedKeyNode());
		unlink(path.c_str());
	}

	private:
		InjectedNode(const InjectedNode &);
		InjectedNode &operator=(const InjectedNode &);
};

/* The device the server installs, pointed at the loop under test. Both globals
   are put back by a destructor rather than by the last lines of a case,
   because a failed assertion unwinds out of one. */
struct InstalledRealInput
{
	explicit InstalledRealInput(CRCInput *rc)
	{
		g_RCInput = rc;
		installRealInputDevice();
	}

	~InstalledRealInput()
	{
		setInputDevice(0);
		g_RCInput = 0;
	}

	private:
		InstalledRealInput(const InstalledRealInput &);
		InstalledRealInput &operator=(const InstalledRealInput &);
};

} // namespace

/* The whole way a key goes: out through the device the server installs, into
   a node, and back off the loop that reads the box's own nodes. Against the
   real loop rather than a fake of it, because what broke here was not what
   either end does with a key but which file each of them named, and only two
   real ends can disagree about that. A build with no input hardware reads one
   file and no /dev/input node at all, and the writer named an event node, so
   every key was answered as one this box does not have.

   Not a case about the container: the node is moved for both ends at once, so
   what this pins is that they move together. */
TEST_CASE("a key the server sends comes back off the loop that reads the box", "[rc]")
{
	InjectedNode node;
	requireFreeEventSocket();

	CRCInput rc;
	InstalledRealInput installed(&rc);

	REQUIRE(osd::sendKey("KEY_0").ok());

	/* Waited on rather than read straight off, because the loop selects and
	   the write went through a fifo. Long enough that a machine under load
	   does not decide this, short enough not to be the run. */
	neutrino_msg_t got = 0;
	neutrino_msg_data_t data = 0;
	rc.getMsg_ms(&got, &data, 2000);

	INFO("the loop answered " << got << " and the key was " << (neutrino_msg_t) CRCInput::RC_0);
	REQUIRE(got == (neutrino_msg_t) CRCInput::RC_0);
	// Pressed and not released: the loop says which of the two in the data
	// word, and a release read as a press is a key acted on twice.
	REQUIRE(data == 0);
}

/* The node the loop opens is the last of the list the writer walks, so a path
   written into one of the two by hand parts them. Read off both ends rather
   than compared with a literal, which is the copy this is about. */
TEST_CASE("both ends of an injected key name one node", "[rc]")
{
	InjectedNode node;

	const std::vector<std::string> nodes = injectedKeyNodes();
	REQUIRE_FALSE(nodes.empty());
	REQUIRE(nodes.back() == node.path);
}
