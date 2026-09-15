/*
 * test_settingsfields.cpp - tests for the settings fields
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

#include "coreapi/base/deps.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"

#include <string>

#include <timerdclient/timerdtypes.h>

/* The kinds of row whose value is not simply a field of the settings struct, driven
   through the layer the way a request drives it.

   The cases beside this one read the program's own source and hold a row to what the
   screens say about it. None of that can say where the value goes: a row declaring a
   bit of a mask and writing the whole mask reads as right on every page there is, and so
   does one that says a daemon holds its value and quietly writes the buffer beside it.
   What that is held to is here, by writing through the layer and looking at what
   moved. */

using namespace coreapi;

namespace
{

// The store the box runs on, because what carries a write out of this layer is
// that store and a fake carries nothing: a case over a fake would be watching
// its own bookkeeping. The sink stands in for the message loop, so a case can
// see the write and the carrying apart.
bool fixtureSaved = false;
bool fixtureSave() { fixtureSaved = true; return true; }

struct RealStore
{
	SNeutrinoSettings     values;
	FakeCommandSink       sink;
	InstalledSink         installed;
	ClearedSettingsSource cleared;

	RealStore() : values(SNeutrinoSettings()), installed(&sink)
	{
		fixtureSaved = false;
		installRealSettingsSource(&values, fixtureSave);
	}

	~RealStore() { installRealSettingsSource(NULL, NULL); }
};

const char *const kBefore = "record_safety_time_before";
const char *const kAfter = "record_safety_time_after";
const char *const kStd = "recording_audio_pids_std";
const char *const kAlt = "recording_audio_pids_alt";
const char *const kAc3 = "recording_audio_pids_ac3";
const char *const kMask = "recording_audio_pids_default";

} // namespace

/* The two safety times, and the one thing no scan of the source could say about
   them: that the value comes from the daemon rather than from the member named
   after it. The member is left at a different number on purpose, so a read that
   took the member would answer that number and this would fail. */
TEST_CASE("a safety time is read from the daemon and not from the member beside it", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	// Nine hundred seconds is fifteen minutes, and the buffer says seven.
	safety.before = 900;
	safety.after = 1800;
	store.values.record_safety_time_before = 7;
	store.values.record_safety_time_after = 7;

	Result<std::string> before = settings::get(kBefore);
	REQUIRE(before.ok());
	CHECK(before.value() == "15");

	Result<std::string> after = settings::get(kAfter);
	REQUIRE(after.ok());
	CHECK(after.value() == "30");

	CHECK(safety.reads > 0);
}

/* The unit, which is the one decision in this row a reader cannot check against
   anything: the daemon keeps seconds and both the television and this offer
   minutes. A row that had passed the daemon's own number through would answer
   nine hundred here. */
TEST_CASE("a safety time is offered in the minutes the television offers", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	safety.before = 60;
	Result<std::string> one = settings::get(kBefore);
	REQUIRE(one.ok());
	CHECK(one.value() == "1");

	REQUIRE(settings::set(kBefore, "3").ok());
	applyPendingSettings();
	CHECK(safety.before == 180);
}

/* A write of one of the pair, which the daemon takes as both. The other has to
   come from the daemon: folding in the member named after it would carry
   whatever a screen last left there into a setting nobody asked about. */
TEST_CASE("writing one safety time leaves the other as the daemon has it", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	safety.before = 120;
	safety.after = 600;
	// What a screen left in the buffers, which must reach neither of them.
	store.values.record_safety_time_before = 42;
	store.values.record_safety_time_after = 42;

	REQUIRE(settings::set(kAfter, "5").ok());
	applyPendingSettings();

	CHECK(safety.after == 300);
	CHECK(safety.before == 120);

	REQUIRE(settings::set(kBefore, "4").ok());
	applyPendingSettings();

	CHECK(safety.before == 240);
	CHECK(safety.after == 300);
}

// The member stays where it was, because nothing here writes it. A write that
// had gone to the member would leave the daemon untouched and this one shows
// both halves of that at once.
TEST_CASE("writing a safety time does not touch the member named after it", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	store.values.record_safety_time_before = 42;

	REQUIRE(settings::set(kBefore, "6").ok());
	applyPendingSettings();

	CHECK(store.values.record_safety_time_before == 42);
	CHECK(safety.before == 360);
}

/* The write reads back before the loop has taken it, as a write of a field does: a
   caller that writes and reads must not be told its own write did nothing. And nothing
   reaches the daemon until the loop runs. Written as a count and not as a value, because
   the fake starts at nought and a write of nought would be indistinguishable from no
   write at all. */
TEST_CASE("a safety time is held until the loop takes it and reads back at once", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	safety.before = 60;

	REQUIRE(settings::set(kBefore, "9").ok());
	CHECK(safety.writes == 0);
	CHECK(safety.before == 60);

	Result<std::string> held = settings::get(kBefore);
	REQUIRE(held.ok());
	CHECK(held.value() == "9");

	applyPendingSettings();
	CHECK(safety.writes == 1);
	CHECK(safety.before == 540);
}

// A daemon nobody can reach is an answer and not a number. Nought is a real
// value of this setting, so answering it would report a recording as starting
// the second the programme does.
TEST_CASE("a safety time whose daemon cannot be reached is not answered as nought", "[settingsfields]")
{
	RealStore store;

	Result<std::string> none = settings::get(kBefore);
	CHECK_FALSE(none.ok());

	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);
	safety.fail_next = true;

	Result<std::string> failed = settings::get(kBefore);
	CHECK_FALSE(failed.ok());

	Result<std::string> answered = settings::get(kBefore);
	REQUIRE(answered.ok());
	CHECK(answered.value() == "0");
}

/* The row's own bounds still hold, and they are the whole of the rule for one
   of these: there is no field behind it whose width could refuse anything. */
TEST_CASE("a safety time is held to the bounds the screen offers", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	CHECK_FALSE(settings::set(kBefore, "100").ok());
	CHECK_FALSE(settings::set(kBefore, "-1").ok());
	CHECK(settings::set(kBefore, "99").ok());
	applyPendingSettings();
	CHECK(safety.before == 99 * 60);
}

// Nobody is told about it afterwards. Every notifier reads the member named
// after the row, which this layer never wrote, so one run for this row would
// apply whatever a screen last left in it over the value that was just written.
TEST_CASE("a written safety time asks no applier", "[settingsfields]")
{
	RealStore store;
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed(&safety);

	FakeApplier recording;
	REQUIRE(registerSettingsApplier("recording", &recording) == Status::Ok);

	REQUIRE(settings::set(kBefore, "2").ok());
	applyPendingSettings();
	CHECK(recording.calls == 0);

	// And the section really is one an applier would otherwise be asked for.
	REQUIRE(settings::set("recording_save_in_channeldir", "1").ok());
	applyPendingSettings();
	CHECK(recording.calls == 1);

	registerSettingsApplier("recording", NULL);
}

/* The three questions the screen splits one mask into. Each has to reach its
   own bit and leave the others where they are: a row writing the whole field
   would read as right everywhere else and would clear the two questions beside
   it every time one of them was answered. */
TEST_CASE("a mask bit is read out of the bit of the mask beside it", "[settingsfields]")
{
	RealStore store;

	store.values.recording_audio_pids_default = TIMERD_APIDS_STD | TIMERD_APIDS_AC3;

	Result<std::string> std_on = settings::get(kStd);
	REQUIRE(std_on.ok());
	CHECK(std_on.value() == "1");

	Result<std::string> alt_off = settings::get(kAlt);
	REQUIRE(alt_off.ok());
	CHECK(alt_off.value() == "0");

	Result<std::string> ac3_on = settings::get(kAc3);
	REQUIRE(ac3_on.ok());
	CHECK(ac3_on.value() == "1");
}

TEST_CASE("writing a mask bit leaves the other bits of the mask standing", "[settingsfields]")
{
	RealStore store;

	store.values.recording_audio_pids_default = TIMERD_APIDS_STD | TIMERD_APIDS_AC3;

	REQUIRE(settings::set(kAlt, "1").ok());
	applyPendingSettings();
	CHECK((int) store.values.recording_audio_pids_default ==
	      (TIMERD_APIDS_STD | TIMERD_APIDS_ALT | TIMERD_APIDS_AC3));

	REQUIRE(settings::set(kStd, "0").ok());
	applyPendingSettings();
	CHECK((int) store.values.recording_audio_pids_default ==
	      (TIMERD_APIDS_ALT | TIMERD_APIDS_AC3));

	REQUIRE(settings::set(kAc3, "0").ok());
	applyPendingSettings();
	CHECK((int) store.values.recording_audio_pids_default == TIMERD_APIDS_ALT);
}

// The mask and its bits are one setting offered two ways, so the two cannot
// disagree: there is one field under them and a write of either shows in both.
TEST_CASE("the mask and its bits are one setting", "[settingsfields]")
{
	RealStore store;

	REQUIRE(settings::set(kMask, "2").ok());
	applyPendingSettings();

	Result<std::string> alt = settings::get(kAlt);
	REQUIRE(alt.ok());
	CHECK(alt.value() == "1");

	Result<std::string> std_off = settings::get(kStd);
	REQUIRE(std_off.ok());
	CHECK(std_off.value() == "0");

	REQUIRE(settings::set(kStd, "1").ok());
	applyPendingSettings();

	Result<std::string> mask = settings::get(kMask);
	REQUIRE(mask.ok());
	CHECK(mask.value() == "3");
}

// A bit takes the two values a bit has, whatever the field under it would hold.
TEST_CASE("a mask bit takes the two values of a bit", "[settingsfields]")
{
	RealStore store;

	CHECK_FALSE(settings::set(kStd, "2").ok());
	CHECK_FALSE(settings::set(kStd, "-1").ok());
	CHECK(settings::set(kStd, "1").ok());
	CHECK(settings::set(kStd, "0").ok());
}

/* The store's own question about a bit, which the row's bounds do not answer
   and which nothing above the store ever reaches: a Bool row is refused a value
   outside its two before the store is asked at all, so the rule the field
   itself carries is only ever the deciding one for a caller that goes straight
   to the store. Driven that way here, or the case would pass with nothing
   behind it. */
TEST_CASE("the store refuses a mask bit a value no bit has", "[settingsfields]")
{
	RealStore store;

	CHECK(settingsSource().writeInt(kStd, 1) == Status::Ok);
	CHECK(settingsSource().writeInt(kStd, 0) == Status::Ok);
	CHECK(settingsSource().writeInt(kStd, 2) == Status::InvalidArgument);
	CHECK(settingsSource().writeInt(kStd, -1) == Status::InvalidArgument);

	// And the field under it really would hold those, so what refused them is
	// the bit's own rule and not the width of the mask.
	CHECK(settingsSource().writeInt(kMask, 2) == Status::Ok);
}

/* The start channel, which is a sixty four bit identifier the long a value
   travels in here is half as wide as. Carried as the text a channel is named by
   everywhere else in this layer, so the one thing to hold it to is that the
   whole of it survives being written and read back: a row that had taken it as
   a number would answer the bottom half of it. */
TEST_CASE("a channel identifier keeps its whole width", "[settingsfields]")
{
	RealStore store;

	// A value with bits above the thirty second, which is what a long on the
	// box could not hold.
	REQUIRE(settings::set("startchanneltv_id", "c0ffee1234abcd").ok());
	applyPendingSettings();

	CHECK(store.values.startchanneltv_id == 0xc0ffee1234abcdULL);

	Result<std::string> back = settings::get("startchanneltv_id");
	REQUIRE(back.ok());
	CHECK(back.value() == "c0ffee1234abcd");
}

// The spelling is the one a listing answers with, so what a caller hands back
// is what it was given. Lower case out, either case in.
TEST_CASE("a channel identifier is spelled the way a channel is named", "[settingsfields]")
{
	RealStore store;

	REQUIRE(settings::set("startchannelradio_id", "ABCDEF").ok());
	applyPendingSettings();
	CHECK(store.values.startchannelradio_id == 0xabcdefULL);

	Result<std::string> back = settings::get("startchannelradio_id");
	REQUIRE(back.ok());
	CHECK(back.value() == "abcdef");
}

/* A spelling nothing can read is refused where the caller is still listening.
   The store would take it as far as the field, drop it there and read back
   whatever the field already held, on a thread with nobody left to answer. */
TEST_CASE("a channel identifier nothing can read is refused before the store", "[settingsfields]")
{
	RealStore store;

	store.values.startchanneltv_id = 0x1234ULL;

	CHECK_FALSE(settings::set("startchanneltv_id", "nonsense").ok());
	CHECK_FALSE(settings::set("startchanneltv_id", "").ok());
	// Seventeen digits, which taken as the last sixteen would name a channel
	// nobody asked for.
	CHECK_FALSE(settings::set("startchanneltv_id", "11111111111111111").ok());

	applyPendingSettings();
	CHECK(store.values.startchanneltv_id == 0x1234ULL);
}

/* The name beside the identifier, which is what the screen shows and not what
   the box zaps to. Two rows over two members, so writing one really does leave
   the other where it was: a frontend that means to change the start channel has
   to write both, and this is what says the layer does not do it for it. */
TEST_CASE("the start channel name and its identifier are two settings", "[settingsfields]")
{
	RealStore store;

	store.values.StartChannelTV = "Das Erste";
	store.values.startchanneltv_id = 0x1234ULL;

	REQUIRE(settings::set("startchanneltv_id", "5678").ok());
	applyPendingSettings();
	CHECK(store.values.startchanneltv_id == 0x5678ULL);
	CHECK(store.values.StartChannelTV == "Das Erste");

	REQUIRE(settings::set("startchanneltv", "ZDF").ok());
	applyPendingSettings();
	CHECK(store.values.StartChannelTV == "ZDF");
	CHECK(store.values.startchanneltv_id == 0x5678ULL);
}

/* The three settings whose set of values the box decides. What no scan of the
   source can say about one of them is whether the set really is asked for, so
   these ask it: a fake stands in for the screen, and what the layer offers and
   what it takes both have to come back to what that fake said. */
TEST_CASE("a setting whose values the box decides is asked for them", "[settingsfields]")
{
	RealStore store;
	FakeSettingChoices screens;
	InstalledSettingChoices installed(&screens);

	screens.offer("video_mode", 5, "720p 50Hz");
	screens.offer("video_mode", 13, "1080p 50Hz");

	Result<std::vector<SettingChoice> > offered = settings::choices("video_Mode");
	REQUIRE(offered.ok());
	REQUIRE(offered.value().size() == 2);
	CHECK(offered.value()[0].value == 5);
	CHECK(offered.value()[0].label == "720p 50Hz");
	CHECK(offered.value()[1].value == 13);
	CHECK(offered.value()[1].label == "1080p 50Hz");
	CHECK(screens.asked > 0);
}

// A value the box has no mode for is refused, and the row's own list is not
// what it is held to: the row carries none.
TEST_CASE("a value the box does not offer is refused", "[settingsfields]")
{
	RealStore store;
	FakeSettingChoices screens;
	InstalledSettingChoices installed(&screens);

	screens.offer("video_mode", 5, "720p 50Hz");
	store.values.video_Mode = 5;

	CHECK(settings::set("video_Mode", "5").ok());
	applyPendingSettings();
	CHECK(store.values.video_Mode == 5);

	CHECK_FALSE(settings::set("video_Mode", "13").ok());
	applyPendingSettings();
	CHECK(store.values.video_Mode == 5);
}

/* Nothing is taken while nobody can say what the set is. Writing a mode the box
   cannot show leaves a picture nobody gets back from with the remote control,
   so the safe direction is to refuse rather than to let anything through. */
TEST_CASE("a setting whose values nobody can state takes none", "[settingsfields]")
{
	RealStore store;
	store.values.video_Mode = 5;

	Result<std::vector<SettingChoice> > none = settings::choices("video_Mode");
	CHECK_FALSE(none.ok());
	CHECK(none.error().code == ErrorCode::ChoicesUnavailable);

	CHECK_FALSE(settings::set("video_Mode", "5").ok());
	applyPendingSettings();
	CHECK(store.values.video_Mode == 5);

	// And an empty answer is the same thing as no answer: a set nothing offers
	// a value for is a setting nothing can be drawn from.
	FakeSettingChoices screens;
	InstalledSettingChoices installed(&screens);
	screens.sets["video_mode"].clear();
	screens.sets["video_mode"];

	Result<std::vector<SettingChoice> > empty = settings::choices("video_Mode");
	CHECK_FALSE(empty.ok());
}

/* A row carrying its own list answers the same shape, with each label already
   the text the box would show: a caller draws a chooser from one answer and
   never has to tell the two kinds apart. */
TEST_CASE("a setting carrying its own list answers the same shape", "[settingsfields]")
{
	RealStore store;
	FakeLocaleSource catalog;
	InstalledLocaleSource installed(&catalog);
	catalog.texts["videomenu.videoformat_43"] = "4:3";
	catalog.texts["videomenu.videoformat_149"] = "14:9";
	catalog.texts["videomenu.videoformat_169"] = "16:9";

	Result<std::vector<SettingChoice> > offered = settings::choices("video_Format");
	REQUIRE(offered.ok());
	REQUIRE(offered.value().size() == 3);
	CHECK(offered.value()[0].value == 1);
	CHECK(offered.value()[0].label == "4:3");
	CHECK(offered.value()[2].value == 3);
	CHECK(offered.value()[2].label == "16:9");
}

// A setting that offers no set at all is the same answer as one nobody can
// state: either way a caller has no list and cannot draw a chooser.
TEST_CASE("a setting that is not a choice offers no values", "[settingsfields]")
{
	RealStore store;

	Result<std::vector<SettingChoice> > number = settings::choices("lcd_scroll");
	CHECK_FALSE(number.ok());
	CHECK(number.error().code == ErrorCode::ChoicesUnavailable);

	Result<std::vector<SettingChoice> > nothing = settings::choices("no-such-key-4711");
	CHECK_FALSE(nothing.ok());
	CHECK(nothing.error().code == ErrorCode::UnknownSetting);
}

/* The size the box draws its own screen at, which the settings struct has a
   member for and does not hold: the program saves the copy another object of it
   keeps, so a row over the member would be a value the next save dropped. Read
   and written through that object, and the member left alone, which is what
   these say. */
TEST_CASE("the drawing size is read beside the settings and not out of them", "[settingsfields]")
{
	RealStore store;
	FakeOsdResolution drawing;
	InstalledOsdResolution installed(&drawing);

	drawing.mode = 1;
	store.values.osd_resolution = 0;

	Result<std::string> read = settings::get("osd_resolution");
	REQUIRE(read.ok());
	CHECK(read.value() == "1");
	CHECK(drawing.reads > 0);
}

TEST_CASE("writing the drawing size reaches the copy the save writes", "[settingsfields]")
{
	RealStore store;
	FakeOsdResolution drawing;
	InstalledOsdResolution installed(&drawing);

	store.values.osd_resolution = 0;

	REQUIRE(settings::set("osd_resolution", "1").ok());
	CHECK(drawing.writes == 0);

	applyPendingSettings();
	CHECK(drawing.writes == 1);
	CHECK(drawing.mode == 1);
	// The member is not what the save writes, so nothing here writes it.
	CHECK(store.values.osd_resolution == 0);
}

// The two modes and nothing between or beyond them.
TEST_CASE("the drawing size takes one of the two modes the program has", "[settingsfields]")
{
	RealStore store;
	FakeOsdResolution drawing;
	InstalledOsdResolution installed(&drawing);

	CHECK(settings::set("osd_resolution", "0").ok());
	CHECK(settings::set("osd_resolution", "1").ok());
	CHECK_FALSE(settings::set("osd_resolution", "2").ok());
	CHECK_FALSE(settings::set("osd_resolution", "-1").ok());
}

// Nobody is told afterwards: the object was changed directly, and the screen's
// own notifier reads the value it is handed rather than the member, so running
// it from here would read a pointer nothing filled.
TEST_CASE("a written drawing size asks no applier", "[settingsfields]")
{
	RealStore store;
	FakeOsdResolution drawing;
	InstalledOsdResolution installed(&drawing);

	FakeApplier osd;
	REQUIRE(registerSettingsApplier("osd", &osd) == Status::Ok);

	REQUIRE(settings::set("osd_resolution", "1").ok());
	applyPendingSettings();
	CHECK(osd.calls == 0);

	REQUIRE(settings::set("mode_icons", "1").ok());
	applyPendingSettings();
	CHECK(osd.calls == 1);

	registerSettingsApplier("osd", NULL);
}

// A screen that has not handed its copy over is an answer and not a mode:
// either mode is a real value of this setting, so answering one would report
// the box as drawing at a size nothing knows it is drawing at.
TEST_CASE("a drawing size nobody keeps is not answered as a mode", "[settingsfields]")
{
	RealStore store;

	Result<std::string> none = settings::get("osd_resolution");
	CHECK_FALSE(none.ok());
}
