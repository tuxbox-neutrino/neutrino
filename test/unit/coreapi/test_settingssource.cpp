/*
 * test_settingssource.cpp - tests for the settings source
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
#include "support/fakes.h"

#include "coreapi/settings/settingsfield.h"
#include "coreapi/settings/settingstable.h"

#include <cstring>

#include <neutrinoMessages.h>

#include <OpenThreads/Block>
#include <OpenThreads/Thread>

using namespace coreapi;

TEST_CASE("the default source answers NotSupported rather than pretending", "[settingssource]")
{
	setSettingsSource(NULL);
	long v = 0;
	REQUIRE(settingsSource().readInt("anything", v) == Status::NotSupported);
	REQUIRE(settingsSource().persist() == Status::NotSupported);
}

// The two reads and the two writes are four refusals and not one, so the three
// the case above does not ask for are asked here.
TEST_CASE("the default source refuses the string read and both writes too", "[settingssource]")
{
	setSettingsSource(NULL);

	std::string s = "untouched";
	REQUIRE(settingsSource().readString("anything", s) == Status::NotSupported);
	// A refused read leaves the answer alone, or a caller that ignores the
	// status reads whatever the refusal wrote there.
	REQUIRE(s == "untouched");

	REQUIRE(settingsSource().writeInt("anything", 1) == Status::NotSupported);
	REQUIRE(settingsSource().writeString("anything", "value") == Status::NotSupported);
}

TEST_CASE("an installed source is the one that answers", "[settingssource]")
{
	FakeSettingsSource f;
	f.ints["volume"] = 42;
	setSettingsSource(&f);

	long v = 0;
	REQUIRE(settingsSource().readInt("volume", v) == Status::Ok);
	REQUIRE(v == 42);

	REQUIRE(settingsSource().readInt("absent", v) == Status::NotFound);
	setSettingsSource(NULL);
}

// The same name in both maps, so a read that went to the wrong kind answers a
// value rather than nothing and is seen.
TEST_CASE("a string read and a string write reach the source as well", "[settingssource]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	f.strings["name"] = "box";
	f.ints["name"] = 1;

	std::string s;
	REQUIRE(settingsSource().readString("name", s) == Status::Ok);
	REQUIRE(s == "box");

	REQUIRE(settingsSource().writeString("name", "other") == Status::Ok);
	REQUIRE(f.strings["name"] == "other");
	REQUIRE(f.ints["name"] == 1);

	REQUIRE(settingsSource().readString("absent", s) == Status::NotFound);
}

TEST_CASE("a write reaches the source and persist is separate from it", "[settingssource]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);

	REQUIRE(settingsSource().writeInt("volume", 7) == Status::Ok);
	REQUIRE(f.ints["volume"] == 7);
	REQUIRE(f.persisted == 0);

	REQUIRE(settingsSource().persist() == Status::Ok);
	REQUIRE(f.persisted == 1);
	setSettingsSource(NULL);
}

/* Taking the source away has to answer the refusing one again. Said in a case of its
   own because every other case here installs something first, so a setter that kept the
   last source would be caught by whichever case ran next and by none of them if that
   case installed one of its own. The fake outlives the removal, so a setter that kept
   it answers through an object that is still there. */
TEST_CASE("taking the source away puts the refusing one back", "[settingssource]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);

	long v = 0;
	REQUIRE(settingsSource().readInt("anything", v) == Status::NotFound);

	setSettingsSource(NULL);
	REQUIRE(settingsSource().readInt("anything", v) == Status::NotSupported);
}

// The fake is what the cases above and the ones the modules bring answer
// through, so the one flag they steer it with is driven here.
TEST_CASE("the fake fails the one call it was told to fail", "[settingssource]")
{
	FakeSettingsSource f;
	InstalledSettingsSource installed(&f);
	f.ints["volume"] = 3;
	f.fail_next = true;

	long v = 0;
	REQUIRE(settingsSource().readInt("volume", v) == Status::Internal);
	REQUIRE(v == 0);

	REQUIRE(settingsSource().readInt("volume", v) == Status::Ok);
	REQUIRE(v == 3);
}

/* The cases below drive the shipped adapter and not a stand in. It needs the program's
   settings struct and a table, both of which a case can build, so unlike the other three
   adapters it links here. Rows of four kinds, because the locator is generated per field
   type and a case set written entirely of ints would pass over a locator that only works
   for one. */
namespace
{
const Descriptor kRows[] =
{
	{
		"repeat_blocker", ValueType::Int, "keybindings", "label", NULL,
		0, 2000, NULL, 0, 450, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	{
		"current_volume", ValueType::Int, "audio", "label", NULL,
		0, 100, NULL, 0, 75, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume)
	},
	{
		"channellist_descmode", ValueType::Bool, "channels", "label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_descmode)
	},
	{
		"language", ValueType::String, "general", "label", NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(language)
	},
	// Declared and not reachable, which is what the program's file only
	// settings look like from here.
	{
		"font_scaling_x", ValueType::Int, "osd", "label", NULL,
		0, 200, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NO_FIELD
	},
};

const size_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);

unsigned saves = 0;
bool save_answer = true;

// What the values held when the save was asked for, so a case can see that the
// writes went in before it and not after.
long saved_repeat_blocker = 0;
std::string saved_language;
const SNeutrinoSettings *saved_from = 0;

bool countSave()
{
	saves++;
	if (saved_from)
	{
		saved_repeat_blocker = saved_from->repeat_blocker;
		saved_language = saved_from->language;
	}
	return save_answer;
}

// The command the source sends to ask the loop for both. Reading it off the
// message the loop answers, so a case cannot agree with a number this file
// made up.
const neutrino_msg_t kApply = NeutrinoMessages::APPLY_SETTINGS;
} // anonymous namespace

TEST_CASE("the real source reads the values the program is running on", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.repeat_blocker = 450;
	values.current_volume = 75;
	values.channellist_descmode = true;
	values.language = "deutsch";

	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 450);
	REQUIRE(settingsSource().readInt("current_volume", v) == Status::Ok);
	REQUIRE(v == 75);
	REQUIRE(settingsSource().readInt("channellist_descmode", v) == Status::Ok);
	REQUIRE(v == 1);

	std::string s;
	REQUIRE(settingsSource().readString("language", s) == Status::Ok);
	REQUIRE(s == "deutsch");
}

// A value the program changes after the source was installed, which is the
// whole point of reading the values rather than the file they were saved to.
TEST_CASE("a value the program changes is the value the next read answers", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.repeat_blocker = 450;

	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 450);

	values.repeat_blocker = 700;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 700);
}

/* A write is held until the loop takes it, because the values belong to the
   loop and several of them are strings. It reads back at once all the same, or
   a caller that writes and reads is told its own write did nothing. */
TEST_CASE("a write is held for the loop and reads back before it lands", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.repeat_blocker = 450;

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().writeInt("current_volume", 30) == Status::Ok);
	REQUIRE(settingsSource().writeInt("channellist_descmode", 1) == Status::Ok);
	REQUIRE(settingsSource().writeString("language", "english") == Status::Ok);

	// Nothing of the program's has moved, and nothing was sent either: a write
	// is not what asks the loop for anything.
	REQUIRE(values.repeat_blocker == 450);
	REQUIRE((int) values.current_volume == 0);
	REQUIRE(values.language == "");
	REQUIRE(sink.posted.empty());

	long v = 0;
	std::string t;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 700);
	REQUIRE(settingsSource().readInt("current_volume", v) == Status::Ok);
	REQUIRE(v == 30);
	REQUIRE(settingsSource().readString("language", t) == Status::Ok);
	REQUIRE(t == "english");

	applyPendingSettings();

	REQUIRE(values.repeat_blocker == 700);
	REQUIRE((int) values.current_volume == 30);
	REQUIRE(values.channellist_descmode == true);
	REQUIRE(values.language == "english");
}

// The last write of a setting is the one the loop takes and the one a read
// answers, or two writes in a request leave the first of them in effect.
TEST_CASE("writing a setting twice keeps the second value", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().writeInt("repeat_blocker", 800) == Status::Ok);

	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 800);

	applyPendingSettings();
	REQUIRE(values.repeat_blocker == 800);
}

/* The one message carries both, so nothing can arrive between the write and the
   save. The program reloads its whole settings on a message of its own, and a
   write that had landed without its save would be thrown away by it. */
TEST_CASE("the writes are in the values before the save is asked for", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.repeat_blocker = 450;

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	saves = 0;
	saved_from = &values;
	saved_repeat_blocker = 0;
	saved_language = "";

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().writeString("language", "english") == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Ok);

	// One message and one only, carrying both the writes and the save.
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].first == kApply);
	REQUIRE(saves == 0);

	applyPendingSettings();

	REQUIRE(saves == 1);
	REQUIRE(saved_repeat_blocker == 700);
	REQUIRE(saved_language == "english");
	saved_from = 0;
}

// A box whose loop will not take the message has not been asked to save, and
// saying Ok would report a setting as kept that is still only held here.
TEST_CASE("a refused message is what persist answers with", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	sink.answer = Status::Busy;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Busy);

	/* Taken back with it. Nothing will carry a batch nobody was told about, so
	   leaving it held would answer a caller that its value was not written and
	   then read that value back, and hand it to whichever message came next. */
	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 0);

	applyPendingSettings();
	REQUIRE(values.repeat_blocker == 0);
}

/* Only its own. A refusal that emptied the whole of what is held would throw
   away a write some other caller has already been told went through. */
TEST_CASE("a refused message takes back its own writes and leaves the rest", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	REQUIRE(settingsSource().writeInt("current_volume", 42) == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Ok);

	sink.answer = Status::Busy;
	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Busy);

	applyPendingSettings();
	REQUIRE(values.current_volume == 42);
	REQUIRE(values.repeat_blocker == 0);
}

/* The fields are narrower than the long a caller hands over, and each row's
   width is its own: what fits the four byte field is refused by the one byte
   field beside it. A source that stored the low bytes would leave a setting at
   a value nobody asked for. */
TEST_CASE("a value too wide for its own field is refused", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.current_volume = 75;
	values.channellist_descmode = false;

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	REQUIRE(settingsSource().writeInt("current_volume", 300) == Status::InvalidArgument);
	// The same number in the wider field of the row above is taken.
	REQUIRE(settingsSource().writeInt("repeat_blocker", 300) == Status::Ok);

	// A bool holds one of two values and nothing else, so anything but those is
	// a value it cannot carry rather than one it rounds.
	REQUIRE(settingsSource().writeInt("channellist_descmode", 2) == Status::InvalidArgument);

	applyPendingSettings();
	REQUIRE((int) values.current_volume == 75);
	REQUIRE(values.repeat_blocker == 300);
	REQUIRE(values.channellist_descmode == false);
}

TEST_CASE("a key no row declares is told apart from one with nowhere to read", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	long v = 0;
	std::string s = "untouched";
	REQUIRE(settingsSource().readInt("nothing_declares_this", v) == Status::NotFound);
	REQUIRE(settingsSource().writeInt("nothing_declares_this", 1) == Status::NotFound);

	// Declared, and its value is not in the struct at all.
	REQUIRE(settingsSource().readInt("font_scaling_x", v) == Status::NotSupported);
	REQUIRE(settingsSource().writeInt("font_scaling_x", 1) == Status::NotSupported);

	// Declared as the other kind, which is not a value this call can answer.
	REQUIRE(settingsSource().readString("repeat_blocker", s) == Status::NotSupported);
	REQUIRE(settingsSource().writeString("repeat_blocker", "x") == Status::NotSupported);
	REQUIRE(settingsSource().readInt("language", v) == Status::NotSupported);
	REQUIRE(settingsSource().writeInt("language", 1) == Status::NotSupported);
	REQUIRE(s == "untouched");
}

// Asking to save with nothing written is still a save: a caller may mean the
// file rather than any setting of its own.
TEST_CASE("persist with nothing written still asks the loop", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	saves = 0;
	REQUIRE(settingsSource().persist() == Status::Ok);
	REQUIRE(sink.posted.size() == 1);

	applyPendingSettings();
	REQUIRE(saves == 1);
}

/* What the loop took is gone from here. A write that stayed would be applied
   again at the next save, over whatever the box has changed since. */
TEST_CASE("what the loop has taken is not applied a second time", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	applyPendingSettings();
	REQUIRE(values.repeat_blocker == 700);

	// The program moves it, as a menu would.
	values.repeat_blocker = 120;
	applyPendingSettings();
	REQUIRE(values.repeat_blocker == 120);
}

// Installing again is the box starting over, so a write nobody carried is not
// carried into the values the second install named.
TEST_CASE("a write nothing took does not follow the next install", "[settingssource]")
{
	SNeutrinoSettings first = SNeutrinoSettings();
	SNeutrinoSettings second = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&first, countSave);
	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);

	installRealSettingsSource(&second, countSave);
	applyPendingSettings();
	REQUIRE(second.repeat_blocker == 0);

	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 0);
}

// Nothing installed, and the loop still runs the message: it has to find no
// values rather than the last ones some case left behind.
TEST_CASE("the loop side does nothing while no source is installed", "[settingssource]")
{
	setSettingsSource(NULL);
	installRealSettingsSource(NULL, countSave);

	saves = 0;
	applyPendingSettings();
	REQUIRE(saves == 0);
	setSettingsSource(NULL);
}

TEST_CASE("a source with no save refuses to save and reads all the same", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.repeat_blocker = 450;

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, NULL);

	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 450);
	REQUIRE(settingsSource().persist() == Status::NotSupported);
	REQUIRE(sink.posted.empty());
}

// Installed with nothing to read, which is the one way the adapter can be
// reached without values and the answer a caller can act on. Not NotFound,
// because that would report a box with no settings at all as one that simply
// does not have this setting.
TEST_CASE("the real source without values refuses every call", "[settingssource]")
{
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(NULL, countSave);

	long v = 0;
	std::string s = "untouched";
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Internal);
	REQUIRE(settingsSource().readString("language", s) == Status::Internal);
	REQUIRE(settingsSource().writeInt("repeat_blocker", 1) == Status::Internal);
	REQUIRE(settingsSource().writeString("language", "x") == Status::Internal);
	REQUIRE(settingsSource().persist() == Status::Internal);
	REQUIRE(s == "untouched");
}

/* The source reads the one table this layer declares, so a table it has and a
   table it does not are no longer two states it can be in. What is left is a
   table that declares nothing, and every key is then one nothing declares
   rather than a fault. */
TEST_CASE("over a table that declares nothing every key is one nobody declared", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, 0);
	installRealSettingsSource(&values, countSave);

	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::NotFound);
	REQUIRE(settingsSource().writeInt("repeat_blocker", 1) == Status::NotFound);
}

/* The table the program installs, driven over the same source. Every row of it
   has to be one a read can answer, or a frontend is offered a setting the box
   cannot produce, and the keybindings among them are what says they need
   nothing of their own. */
TEST_CASE("every row the program declares is sane and reachable", "[settingssource]")
{
	REQUIRE(settingsTableCount() > 0);

	SNeutrinoSettings values = SNeutrinoSettings();
	ClearedSettingsSource cleared;
	installRealSettingsSource(&values, countSave);

	/* A row whose value a daemon holds is reachable when the daemon is, and on
	   a box it always is. Installed here so that the loop below asks the same
	   question of those rows as of every other: whether a read answers. Without
	   it they would answer NotSupported and the case would be measuring which
	   seams this binary wires up rather than whether the table is reachable. */
	FakeRecordingSafety safety;
	InstalledRecordingSafety installed_safety(&safety);
	FakeOsdResolution drawing;
	InstalledOsdResolution installed_drawing(&drawing);

	unsigned keybindings = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		INFO("row " << i << " key " << d.key);
		REQUIRE(descriptorIsSane(d));

		long v = 0;
		std::string s;
		if (d.type == ValueType::String)
			REQUIRE(settingsSource().readString(d.key, s) == Status::Ok);
		else
			REQUIRE(settingsSource().readInt(d.key, v) == Status::Ok);

		if (std::strcmp(d.section, "keybindings") == 0)
			keybindings++;
	}

	// A keybinding is an ordinary row here, and saying so needs one to be
	// declared: with none in the table the loop above would prove nothing
	// about them.
	REQUIRE(keybindings > 0);
}

// A keybinding is read and written through the same two calls as anything else,
// because the program keeps it in the same struct and saves it out of the same
// store.
TEST_CASE("a keybinding is written like any other setting", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();
	values.repeat_blocker = 450;

	ClearedSettingsSource cleared;
	installRealSettingsSource(&values, countSave);

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	saves = 0;
	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Ok);

	applyPendingSettings();
	REQUIRE(values.repeat_blocker == 700);
	REQUIRE(saves == 1);
}

/* The two cases below are the only ones here that run two callers at once, and they
   exist because the writes are held in one place while a post is answered per call. A
   post that promised everything held would take a second caller's write with it and,
   when the loop refused the message, throw that write away. Reading the code is not
   enough to say which write a refusal takes back, so both callers are run. */
namespace
{
/* One caller on a thread of its own: a write, a wait, a persist. The case puts its own
   write and its own post inside the wait, so which answer belongs to which caller is
   settled by the run rather than argued.

   Nothing here checks anything. Catch keeps its own state without a lock, so an
   assertion from a second thread is not a check but a race. */
struct OtherCaller : public OpenThreads::Thread
{
	const char *key;
	long value;
	OpenThreads::Block *wrote;
	OpenThreads::Block *go;
	OpenThreads::Block *done;
	Status write_answer;
	Status persist_answer;

	OtherCaller()
		: key(0), value(0), wrote(0), go(0), done(0),
		  write_answer(Status::Internal), persist_answer(Status::Internal) {}

	void run()
	{
		write_answer = settingsSource().writeInt(key, value);
		wrote->release();
		// A ceiling rather than a wait without one, so a case that never lets
		// this go fails the run instead of stopping it.
		go->block(5000);
		persist_answer = settingsSource().persist();
		done->release();
	}
};

const unsigned long kHandshakeMs = 5000;
} // anonymous namespace

TEST_CASE("a refused message leaves the write another caller is still holding", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);
	saves = 0;

	OpenThreads::Block wrote;
	OpenThreads::Block go;
	OpenThreads::Block done;
	OtherCaller other;
	other.key = "current_volume";
	other.value = 42;
	other.wrote = &wrote;
	other.go = &go;
	other.done = &done;
	other.start();

	// Its write is held here and nobody has promised it to the loop yet.
	REQUIRE(wrote.block(kHandshakeMs));

	// This caller writes its own setting and the loop will not take the
	// message, so this caller is told the value was not written.
	sink.answer = Status::Busy;
	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Busy);

	// What the refusal took back is this caller's write and not the other's.
	long v = 0;
	REQUIRE(settingsSource().readInt("repeat_blocker", v) == Status::Ok);
	REQUIRE(v == 0);
	REQUIRE(settingsSource().readInt("current_volume", v) == Status::Ok);
	REQUIRE(v == 42);

	sink.answer = Status::Ok;
	go.release();
	other.join();

	REQUIRE(other.write_answer == Status::Ok);
	REQUIRE(other.persist_answer == Status::Ok);

	applyPendingSettings();

	// The value the other caller was answered ok for is in the box, and the
	// one this caller was refused is not.
	REQUIRE((int) values.current_volume == 42);
	REQUIRE(values.repeat_blocker == 0);
	REQUIRE(saves == 1);
}

/* The same two callers the other way round, so that what holds is the ownership
   of a write and not which thread happens to be the case's own. */
TEST_CASE("a caller whose message was taken keeps its write when another caller is refused", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	FakeCommandSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);
	saves = 0;

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);

	OpenThreads::Block wrote;
	OpenThreads::Block go;
	OpenThreads::Block done;
	OtherCaller other;
	other.key = "current_volume";
	other.value = 42;
	other.wrote = &wrote;
	other.go = &go;
	other.done = &done;
	other.start();

	REQUIRE(wrote.block(kHandshakeMs));

	// The other caller is the one refused this time.
	sink.answer = Status::Busy;
	go.release();
	REQUIRE(done.block(kHandshakeMs));

	sink.answer = Status::Ok;
	REQUIRE(settingsSource().persist() == Status::Ok);

	other.join();
	REQUIRE(other.persist_answer == Status::Busy);

	applyPendingSettings();

	REQUIRE(values.repeat_blocker == 700);
	REQUIRE((int) values.current_volume == 0);
	REQUIRE(saves == 1);
}

/* The one thing a refused message cannot promise. The loop takes everything held when a
   message arrives, and a second caller's message can be draining the store at the moment
   this caller's is refused. The drain is put exactly there rather than reasoned about.
   What this pins is the answer, which is the one that claims less than happened: this
   call did not get the box asked, and it cannot say that nobody else did. */
namespace
{
struct DrainingSink : public CommandSink
{
	unsigned posts;

	DrainingSink() : posts(0) {}

	Status post(neutrino_msg_t, neutrino_msg_data_t)
	{
		++posts;
		applyPendingSettings();
		return Status::Busy;
	}
};
} // anonymous namespace

TEST_CASE("a refused message does not promise that nothing landed", "[settingssource]")
{
	SNeutrinoSettings values = SNeutrinoSettings();

	DrainingSink sink;
	InstalledSink installed(&sink);
	ClearedSettingsSource cleared;
	InstalledSettingsTable table(kRows, kRowCount);
	installRealSettingsSource(&values, countSave);
	saves = 0;
	saved_from = 0;

	REQUIRE(settingsSource().writeInt("repeat_blocker", 700) == Status::Ok);
	REQUIRE(settingsSource().persist() == Status::Busy);
	REQUIRE(sink.posts == 1);

	// Answered not written, and in the box all the same.
	REQUIRE(values.repeat_blocker == 700);
	REQUIRE(saves == 1);
}
