/*
 * test_system.cpp - tests for box facts and power
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

#include "coreapi/decryption.h"
#include "coreapi/osd.h"
#include "coreapi/system.h"

#include <neutrinoMessages.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

using namespace coreapi;

TEST_CASE("each power command names an event of its own", "[system]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(system::reboot().ok());
	REQUIRE(system::shutdown().ok());
	REQUIRE(system::restart().ok());

	REQUIRE(sink.sent.size() == 3);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::REBOOT);
	REQUIRE(sink.sent[1].id == (unsigned) NeutrinoMessages::SHUTDOWN);
	REQUIRE(sink.sent[2].id == (unsigned) NeutrinoMessages::RESTART);

	// None of the three carries anything: the loop reads the whole command out
	// of the id, and a body would be a block it never frees.
	REQUIRE(sink.sent[0].body.empty());
	REQUIRE(sink.sent[1].body.empty());
	REQUIRE(sink.sent[2].body.empty());
}

TEST_CASE("the two directions of standby are two events", "[system]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(system::standby(true).ok());
	REQUIRE(system::standby(false).ok());

	REQUIRE(sink.sent.size() == 2);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::STANDBY_ON);
	REQUIRE(sink.sent[1].id == (unsigned) NeutrinoMessages::STANDBY_OFF);
}

TEST_CASE("the two directions of the television cable are two events", "[system]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(system::hdmiCec(true).ok());
	REQUIRE(system::hdmiCec(false).ok());

	REQUIRE(sink.sent.size() == 2);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON);
	REQUIRE(sink.sent[1].id == (unsigned) NeutrinoMessages::EVT_HDMI_CEC_STANDBY);
}

/* The real sink walks an empty list of clients without a word, so an event it
   was not built to carry would leave the caller with an ok answer and nothing
   done. It reports NotSupported instead, and this pins that the domain hands
   that answer on rather than smoothing it into an ok. */
TEST_CASE("an event the box does not carry is reported and not swallowed", "[system]")
{
	FakeEventSink sink;
	sink.unsupported = NeutrinoMessages::REBOOT;
	InstalledEventSink installed(&sink);

	Result<void> r = system::reboot();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotSupported);
	REQUIRE(sink.sent.empty());

	// Only the one named id is refused, or the case would pass against a sink
	// that refused everything.
	REQUIRE(system::shutdown().ok());
	REQUIRE(sink.sent.size() == 1);
}

/* A send that could not be handed to the socket is the one failure this
   transport can report, and the command did not go out. The domain has to pass
   that on rather than answer for a box it did not reach. */
TEST_CASE("a send that did not reach the box is reported as a failure", "[system]")
{
	FakeEventSink sink;
	sink.answer = Status::Internal;
	InstalledEventSink installed(&sink);

	Result<void> r = system::shutdown();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(sink.sent.empty());
}

/* Count is a length and not a command, and a number cast in from outside the
   enum is not one either. Neither can be answered by reading anything: there is
   no table behind this, only a case per command, so what these pin is that both
   come back as the number no message carries. */
TEST_CASE("a value that is not a command is refused", "[system]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(eventNumber(BoxEvent::Count) == 0);
	REQUIRE(eventNumber((BoxEvent) ((int) BoxEvent::Count + 7)) == 0);
	REQUIRE(eventNumber((BoxEvent) -1) == 0);

	Result<void> r = postEvent(BoxEvent::Count);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(sink.sent.empty());
}

/* The sink registers what it will carry by walking this list, so a number
   missing from it is an event nothing registered and a duplicate is two
   commands sharing one registration. Neither shows up at a call site. */
TEST_CASE("every command has a number of its own", "[system]")
{
	std::vector<unsigned> seen;
	for (size_t i = 0; i < (size_t) BoxEvent::Count; i++)
	{
		unsigned n = eventNumber((BoxEvent) i);
		REQUIRE(n != 0);
		for (size_t j = 0; j < seen.size(); j++)
			REQUIRE(seen[j] != n);
		seen.push_back(n);
	}
	REQUIRE(seen.size() == (size_t) BoxEvent::Count);
}

TEST_CASE("the box facts are handed on as the source gave them", "[system]")
{
	FakeSystemSource source;
	source.box.vendor        = "CST";
	source.box.model         = "Neo Twin";
	source.box.chipset       = "arm";
	source.box.image_version = "4.00";
	source.box.kernel        = "4.10.12";
	source.box.hostname      = "wohnzimmer";
	source.box.uptime        = 4321;
	InstalledSystemSource installed(&source);

	Result<BoxInfo> r = system::info();
	REQUIRE(r.ok());
	BoxInfo got = r.value();
	REQUIRE(got.vendor == "CST");
	REQUIRE(got.model == "Neo Twin");
	REQUIRE(got.chipset == "arm");
	REQUIRE(got.image_version == "4.00");
	REQUIRE(got.kernel == "4.10.12");
	REQUIRE(got.hostname == "wohnzimmer");
	REQUIRE(got.uptime == 4321);
}

TEST_CASE("a box that cannot be identified is a failure and not an empty box", "[system]")
{
	FakeSystemSource source;
	source.box_status = Status::Internal;
	source.box.model = "should not be read";
	InstalledSystemSource installed(&source);

	Result<BoxInfo> r = system::info();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
}

TEST_CASE("a volume outside the percentage is refused before anything is sent", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(osd::setVolume(50).ok());

	Result<void> low = osd::setVolume(-1);
	REQUIRE_FALSE(low.ok());
	REQUIRE(low.error().status == Status::InvalidArgument);

	Result<void> high = osd::setVolume(101);
	REQUIRE_FALSE(high.ok());
	REQUIRE(high.error().status == Status::InvalidArgument);

	// The accepted one alone, so the refusals are refusals and not sends the
	// box happened to ignore.
	REQUIRE(sink.sent.size() == 1);
}

/* The loop reads the data word back out of the body as a signed char, so the
   body has to be that one byte and has to hold the number the caller named.
   Both ends of the range are asked for, because a cast that went wrong at one
   of them still holds at the middle. */
TEST_CASE("the volume travels as the single byte the loop reads back", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(osd::setVolume(0).ok());
	REQUIRE(osd::setVolume(100).ok());

	REQUIRE(sink.sent.size() == 2);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::EVT_SET_VOLUME);
	REQUIRE(sink.sent[0].body.size() == 1);
	REQUIRE((int) (signed char) sink.sent[0].body[0] == 0);
	REQUIRE(sink.sent[1].body.size() == 1);
	REQUIRE((int) (signed char) sink.sent[1].body[0] == 100);
}

TEST_CASE("muting and unmuting travel as one byte of their own event", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(osd::setMuted(true).ok());
	REQUIRE(osd::setMuted(false).ok());

	REQUIRE(sink.sent.size() == 2);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::EVT_SET_MUTE);
	REQUIRE(sink.sent[0].body.size() == 1);
	REQUIRE(sink.sent[0].body[0] == 1);
	REQUIRE(sink.sent[1].body[0] == 0);
}

TEST_CASE("an empty message is refused before anything is sent", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	Result<void> r = osd::message(osd::MessageKind::Hint, "");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(sink.sent.empty());
}

/* The length a caller sends becomes an allocation and a read of exactly that
   many bytes at the other end, both on the thread the box draws on, so the
   caller does not get to name it. */
TEST_CASE("a message longer than the box will draw is refused before anything is sent", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	Result<void> at_the_bound =
		osd::message(osd::MessageKind::Box, std::string(osd::MAX_MESSAGE_BYTES, 'a'));
	REQUIRE(at_the_bound.ok());
	REQUIRE(sink.sent.size() == 1);

	Result<void> past_it =
		osd::message(osd::MessageKind::Box, std::string(osd::MAX_MESSAGE_BYTES + 1, 'a'));
	REQUIRE_FALSE(past_it.ok());
	REQUIRE(past_it.error().status == Status::InvalidArgument);
	REQUIRE(past_it.error().code == ErrorCode::MessageTooLong);
	// Nothing beyond the one that was inside the bound.
	REQUIRE(sink.sent.size() == 1);

	// Far past it, because a bound that only catches the next byte catches
	// nothing a caller who means harm would send.
	Result<void> enormous =
		osd::message(osd::MessageKind::Hint, std::string(8u * 1024u * 1024u, 'a'));
	REQUIRE_FALSE(enormous.ok());
	REQUIRE(enormous.error().code == ErrorCode::MessageTooLong);
	REQUIRE(sink.sent.size() == 1);
}

/* The loop hands the block on as a string and frees it afterwards, so the
   terminator has to be inside what was sent. Nothing else marks the end. */
TEST_CASE("a message carries its terminator with it", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(osd::message(osd::MessageKind::Box, "Guten Abend").ok());

	REQUIRE(sink.sent.size() == 1);
	REQUIRE(sink.sent[0].body.size() == std::string("Guten Abend").size() + 1);
	REQUIRE(sink.sent[0].body[sink.sent[0].body.size() - 1] == 0);
	REQUIRE(std::string(sink.sent[0].body.c_str()) == "Guten Abend");
}

TEST_CASE("the two ways of showing a message are two events", "[osd]")
{
	FakeEventSink sink;
	InstalledEventSink installed(&sink);

	REQUIRE(osd::message(osd::MessageKind::Hint, "kurz").ok());
	REQUIRE(osd::message(osd::MessageKind::Box, "kurz").ok());

	REQUIRE(sink.sent.size() == 2);
	REQUIRE(sink.sent[0].id == (unsigned) NeutrinoMessages::EVT_POPUP);
	REQUIRE(sink.sent[1].id == (unsigned) NeutrinoMessages::EVT_EXTMSG);
}

TEST_CASE("the volume and the mute state are read through the source", "[osd]")
{
	FakeSystemSource source;
	source.volume_level = 33;
	source.is_muted = true;
	InstalledSystemSource installed(&source);

	Result<int> v = osd::volume();
	REQUIRE(v.ok());
	REQUIRE(v.value() == 33);

	Result<bool> m = osd::muted();
	REQUIRE(m.ok());
	REQUIRE(m.value() == true);
}

/* A volume of zero is what a silent box reads and also what an unreadable one
   would leave behind, so the failure has to arrive as a failure. */
TEST_CASE("an unreadable volume is a failure and not a zero", "[osd]")
{
	FakeSystemSource source;
	source.volume_status = Status::Internal;
	source.volume_level = 77;
	InstalledSystemSource installed(&source);

	Result<int> v = osd::volume();
	REQUIRE_FALSE(v.ok());
	REQUIRE(v.error().status == Status::Internal);

	// The mute read has its own status and is untouched by the one above.
	Result<bool> m = osd::muted();
	REQUIRE(m.ok());
}

namespace
{

/* A directory of its own to put the softcam's file in, and the path put back
   the way it was found. The machine running the suite has a /tmp already, and
   a case reading the real name would answer differently on a machine where a
   softcam happens to be running. */
struct EcmFixture
{
	std::string dir;
	std::string before;

	EcmFixture()
	{
		char tmpl[] = "/tmp/coreapi_decryption_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		before = decryption::ecmInfoPath();
		decryption::setEcmInfoPath(path());
	}

	~EcmFixture()
	{
		decryption::setEcmInfoPath(before);
		// Named from the global namespace: this file is inside the domain's
		// own system namespace, and the bare name reaches both.
		if (!dir.empty())
			(void) ::system(("rm -rf " + dir).c_str());
	}

	std::string path() const { return dir + "/ecm.info"; }

	void write(const char *text) const
	{
		FILE *f = fopen(path().c_str(), "w");
		REQUIRE(f != NULL);
		if (text[0] != 0)
			REQUIRE(fputs(text, f) >= 0);
		REQUIRE(fclose(f) == 0);
	}

	void remove() const { (void) unlink(path().c_str()); }
};

} // namespace

TEST_CASE("a softcam is answering only while the file it writes has something in it", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	InstalledSystemSource installed(&source);

	// Nothing there at all, which is what a box with no softcam running reads
	// and also what one reads between channels.
	Result<decryption::State> none = decryption::state();
	REQUIRE(none.ok());
	REQUIRE_FALSE(none.value().softcam);

	fx.write("caid: 1801\n");
	Result<decryption::State> answering = decryption::state();
	REQUIRE(answering.ok());
	REQUIRE(answering.value().softcam);

	/* Opened and not yet written to. A length of nought says nothing was
	   answered, and reading presence alone would call that a softcam at work. */
	fx.write("");
	Result<decryption::State> empty = decryption::state();
	REQUIRE(empty.ok());
	REQUIRE_FALSE(empty.value().softcam);

	// And gone again, because a softcam removes it when it stops descrambling.
	fx.remove();
	Result<decryption::State> stopped = decryption::state();
	REQUIRE(stopped.ok());
	REQUIRE_FALSE(stopped.value().softcam);
}

/* A directory of that name is not a file a softcam wrote, and its size is
   whatever the filesystem keeps a directory in rather than a count of bytes
   anybody answered with. */
TEST_CASE("something that is not a file is not a softcam answering", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	InstalledSystemSource installed(&source);

	REQUIRE(mkdir(fx.path().c_str(), 0700) == 0);
	Result<decryption::State> r = decryption::state();
	REQUIRE(r.ok());
	REQUIRE_FALSE(r.value().softcam);
}

TEST_CASE("a module counts only where the box is sending the channel through it", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 1;
	source.ci_seated.push_back(true);
	InstalledSystemSource installed(&source);

	source.ci_used = true;
	Result<decryption::State> both = decryption::state();
	REQUIRE(both.ok());
	REQUIRE(both.value().ci_module);

	// Seated and descrambling nothing, which is a card in a slot the box is
	// not using.
	source.ci_used = false;
	Result<decryption::State> idle = decryption::state();
	REQUIRE(idle.ok());
	REQUIRE_FALSE(idle.value().ci_module);

	// And the other way round: the box set to use one with the slot empty.
	source.ci_seated[0] = false;
	source.ci_used = true;
	Result<decryption::State> empty_slot = decryption::state();
	REQUIRE(empty_slot.ok());
	REQUIRE_FALSE(empty_slot.value().ci_module);
}

/* Every box built without a common interface, which is most of them. It has to
   answer no module rather than fail, and it must not be told apart from a box
   with an empty slot by anything but the reading that was never taken. */
TEST_CASE("a box with no slot answers no module and reads no slot", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 0;
	source.ci_used = true;
	InstalledSystemSource installed(&source);

	Result<decryption::State> r = decryption::state();
	REQUIRE(r.ok());
	REQUIRE_FALSE(r.value().ci_module);
	REQUIRE(source.ci_asked.empty());
}

TEST_CASE("the walk over the slots stops at the module it finds", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 3;
	source.ci_seated.push_back(false);
	source.ci_seated.push_back(true);
	source.ci_seated.push_back(true);
	source.ci_used = true;
	InstalledSystemSource installed(&source);

	Result<decryption::State> r = decryption::state();
	REQUIRE(r.ok());
	REQUIRE(r.value().ci_module);

	// The empty first slot and the one after it, and not the third.
	REQUIRE(source.ci_asked.size() == 2);
	REQUIRE(source.ci_asked[0] == 0);
	REQUIRE(source.ci_asked[1] == 1);
}

/* The surface this replaces asked whether a module was there on its first pass
   and never again, with a note beside it saying so. A person pushing a card in
   while the display is open is the whole of what this answer is for. */
TEST_CASE("a module pushed in while the box runs is read on the next call", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 1;
	source.ci_seated.push_back(false);
	source.ci_used = true;
	InstalledSystemSource installed(&source);

	Result<decryption::State> before = decryption::state();
	REQUIRE(before.ok());
	REQUIRE_FALSE(before.value().ci_module);

	source.ci_seated[0] = true;
	Result<decryption::State> after = decryption::state();
	REQUIRE(after.ok());
	REQUIRE(after.value().ci_module);

	// And out again, or the case would pass against a reading taken once on
	// the second call instead of the first.
	source.ci_seated[0] = false;
	Result<decryption::State> pulled = decryption::state();
	REQUIRE(pulled.ok());
	REQUIRE_FALSE(pulled.value().ci_module);
}

/* No channel, no module in use and no softcam answering for anything. Three
   falses are the truth about such a box and the route has to reach them rather
   than refuse. */
TEST_CASE("a box with nothing on the screen answers rather than refuses", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 1;
	source.ci_seated.push_back(true);
	source.ci_used = false;
	InstalledSystemSource installed(&source);

	Result<decryption::State> r = decryption::state();
	REQUIRE(r.ok());
	REQUIRE_FALSE(r.value().softcam);
	REQUIRE_FALSE(r.value().ci_module);
}

/* False is what a box with nothing seated reads, so a reading that could not
   be taken has to arrive as a failure rather than as that same false. */
TEST_CASE("a conditional access layer that cannot be read is a failure and not a false", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 1;
	source.ci_seated.push_back(true);
	source.ci_used = true;
	InstalledSystemSource installed(&source);

	source.ci_slot_status = Status::Internal;
	Result<decryption::State> slots = decryption::state();
	REQUIRE_FALSE(slots.ok());
	REQUIRE(slots.error().status == Status::Internal);
	REQUIRE(slots.error().code == ErrorCode::DecryptionUnreadable);

	// The other read has a status of its own and is untouched by the one above.
	source.ci_slot_status = Status::Ok;
	source.ci_use_status = Status::Internal;
	Result<decryption::State> used = decryption::state();
	REQUIRE_FALSE(used.ok());
	REQUIRE(used.error().status == Status::Internal);
	REQUIRE(used.error().code == ErrorCode::DecryptionUnreadable);

	// And with neither failing it answers, so the two above are the refusals
	// and not a fixture that refuses everything.
	source.ci_use_status = Status::Ok;
	REQUIRE(decryption::state().ok());
}

/* How many slots there are and what is in one of them are two calls, and a
   failure at the second is inside a loop where an answer is already half built.
   Left unread it would be a box that looks like it has an empty slot. */
TEST_CASE("a slot that cannot be read is a failure even where the count could be", "[decryption]")
{
	EcmFixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	FakeSystemSource source;
	source.ci_slots = 2;
	source.ci_seated.push_back(false);
	source.ci_seated.push_back(true);
	source.ci_used = true;
	InstalledSystemSource installed(&source);

	Result<decryption::State> good = decryption::state();
	REQUIRE(good.ok());
	REQUIRE(good.value().ci_module);

	source.ci_present_status = Status::NotSupported;
	Result<decryption::State> bad = decryption::state();
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::NotSupported);
	REQUIRE(bad.error().code == ErrorCode::DecryptionUnreadable);
}
