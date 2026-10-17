/*
 * test_available.cpp - tests for the settings only some boxes have, and for the lock
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/catch.hpp"

#include "coreapi/base/errors.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/predicates.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"

#include <set>
#include <string>

using namespace coreapi;

namespace
{

struct RowTest
{
	const char *key;
	bool (*test)();
};

/* Every row of the tables that asks the box, with the test it asks. Some are
   declared only in builds for boxes that can have what they control: a second
   picture, the module delay of one family, the video controls of the arm and
   mips boxes. So a case takes the ones this build declares. */
const RowTest kRows[] =
{
	{ "analog_mode2", hasAnalogCinch },
	{ "backlight_deepstandby", hasBacklight },
	{ "backlight_standby", hasBacklight },
	{ "backlight_tv", hasBacklight },
	{ "ci_delay", hasCiSlot },
	{ "ci_standby_reset", hasCiSlot },
	{ "cpufreq", canCpufreq },
	{ "fan_speed", hasFan },
	{ "hdmi_cec_mode", canCec },
	{ "hdmi_cec_standby", canCec },
	{ "hdmi_cec_view_on", canCec },
	{ "hdmi_cec_volume", canCec },
	{ "hdmi_colorimetry", takesHdmiColorimetry },
	{ "hdmi_dd", hasHdmi },
	{ "infobar_show_tuner", severalTunersFitted },
	{ "key_format_mode_active", hasFormatButton },
	{ "lcd_dim_brightness", canSetPanelBrightness },
	{ "lcd_dim_time", canSetPanelBrightness },
	{ "lcd_info_line", vfdEnabled },
	{ "lcd_notify_rclock", vfdEnabled },
	{ "led_blink", hasLedMenu },
	{ "led_deep_mode", hasLedMenu },
	{ "led_rec_mode", hasLedMenu },
	{ "led_standby_mode", hasLedMenu },
	{ "led_tv_mode", hasLedMenu },
	{ "movieplayer_display_playtime", displayFitsPlaytime },
	{ "pip_height", pipUsable },
	{ "pip_radio_height", pipUsable },
	{ "pip_radio_width", pipUsable },
	{ "pip_radio_x", pipUsable },
	{ "pip_radio_y", pipUsable },
	{ "pip_rotate_lastpos", pipUsable },
	{ "pip_width", pipUsable },
	{ "pip_x", pipUsable },
	{ "pip_y", pipUsable },
	{ "remote_control_hardware", canSelectRemote },
	{ "shutdown_block_while_recording", canShutdown },
	{ "standby_cpufreq", canCpufreq },
	{ "video_dbdr", hasDbdr },
	{ "zappingmode", takesZappingMode },
};

const size_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);

bool declaredHere(const char *key)
{
	return settings::describe(key).ok();
}

/* What a box that has everything still lacks in this build: the newer library of
   one family drives SCART and Cinch together, so it has no Cinch row of its own. */
bool lackedInThisBuild(const char *key)
{
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
	return std::string(key) == "analog_mode2";
#else
	(void) key;
	return false;
#endif
}

bool onThisBox(const char *key)
{
	Result<Descriptor> d = settings::describe(key);
	REQUIRE(d.ok());
	return settings::availableNow(d.value());
}

} // anonymous namespace

/* The rows above and no others carry a test, each its own: a row whose test was
   lost is offered on every box, and one that gained a test nobody listed is gone
   from some box nobody asked about. */
TEST_CASE("the rows that ask the box are the ones listed and each asks its own test", "[available]")
{
	std::set<std::string> listed;
	for (size_t i = 0; i < kRowCount; ++i)
	{
		if (declaredHere(kRows[i].key))
			listed.insert(kRows[i].key);
	}
	// Most of them are on every build.
	REQUIRE(listed.size() >= 25);

	const Descriptor *t = settingsTable();
	std::set<std::string> asking;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (t[i].field.available != NULL)
			asking.insert(t[i].key);
	}
	CHECK(asking == listed);

	for (size_t i = 0; i < kRowCount; ++i)
	{
		INFO(kRows[i].key);
		Result<Descriptor> d = settings::describe(kRows[i].key);
		if (d.ok())
			CHECK(d.value().field.available == kRows[i].test);
	}
}

TEST_CASE("a box that has everything has every row, and one that cannot say has none of them", "[available]")
{
	FakeSystemSource source;
	source.hasEverything();
	{
		InstalledSystemSource installed(&source);
		for (size_t i = 0; i < kRowCount; ++i)
		{
			INFO(kRows[i].key);
			if (declaredHere(kRows[i].key))
				CHECK(onThisBox(kRows[i].key) != lackedInThisBuild(kRows[i].key));
		}

		source.caps_status = Status::Internal;
		source.ci_slot_status = Status::Internal;
		for (size_t i = 0; i < kRowCount; ++i)
		{
			INFO(kRows[i].key);
			if (declaredHere(kRows[i].key))
				CHECK_FALSE(onThisBox(kRows[i].key));
		}
	}

	// No box behind the layer at all is a box that cannot say, and nothing ends.
	for (size_t i = 0; i < kRowCount; ++i)
	{
		INFO(kRows[i].key);
		if (declaredHere(kRows[i].key))
			CHECK_FALSE(onThisBox(kRows[i].key));
	}
	// A row that asks nothing is on every box.
	CHECK(onThisBox("audio_AnalogMode"));
}

/* The rows that ask the box through one of the predicates above, each held to
   its own: a row wired to a neighbour's test would still be offered somewhere,
   which no case on the tests alone can see. Every combination of what the tests
   read is walked, so two of them cannot agree by accident. */
TEST_CASE("each row behind a box test is on the box exactly where its test holds", "[available]")
{
	const RowTest rows[] =
	{
		{ "video_dbdr", hasDbdr },
		{ "led_tv_mode", hasLedMenu },
		{ "led_standby_mode", hasLedMenu },
		{ "led_deep_mode", hasLedMenu },
		{ "led_rec_mode", hasLedMenu },
		{ "led_blink", hasLedMenu },
		{ "backlight_tv", hasBacklight },
		{ "backlight_standby", hasBacklight },
		{ "backlight_deepstandby", hasBacklight },
		{ "lcd_dim_brightness", canSetPanelBrightness },
		{ "lcd_dim_time", canSetPanelBrightness },
		{ "lcd_info_line", vfdEnabled },
		{ "lcd_notify_rclock", vfdEnabled },
		{ "remote_control_hardware", canSelectRemote },
		{ "infobar_show_tuner", severalTunersFitted },
	};

	FakeSystemSource source;
	InstalledSystemSource installed(&source);

	const unsigned revisions[] = { 0, 1, 6, 8, 9, 10, 11 };
	for (size_t r = 0; r < sizeof(revisions) / sizeof(revisions[0]); ++r)
	for (int panel = 0; panel < 2; ++panel)
	for (int remote = 0; remote < 2; ++remote)
	for (size_t tuners = 0; tuners < 3; ++tuners)
	{
		source.caps.board_revision = revisions[r];
		source.caps.display_can_set_brightness = panel;
		source.caps.rc_hw_select = remote;
		source.caps.frontend_count = (int) tuners;

		for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			INFO(rows[i].key << " revision " << revisions[r] << " panel " << panel
			     << " remote " << remote << " tuners " << tuners);
			CHECK(onThisBox(rows[i].key) == rows[i].test());
		}
	}
}

TEST_CASE("a write of a row the box lacks is refused and changes nothing", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource source;
	InstalledSystemSource installed(&source);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;

	source.caps.has_fan = 0;
	Result<void> r = settings::set("fan_speed", "3");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().status == Status::Conflict);
	CHECK(r.error().code == ErrorCode::SettingNotOnThisBox);
	Result<void> c = settings::check("fan_speed", "3");
	REQUIRE_FALSE(c.ok());
	CHECK(c.error().code == ErrorCode::SettingNotOnThisBox);
	CHECK(store.ints.count("fan_speed") == 0);

	// The box answers ahead of the value: one it would refuse anyway is refused for the box.
	Result<void> bad = settings::set("fan_speed", "not a number");
	REQUIRE_FALSE(bad.ok());
	CHECK(bad.error().code == ErrorCode::SettingNotOnThisBox);

	source.caps.has_fan = 1;
	REQUIRE(settings::set("fan_speed", "3").ok());
	CHECK(store.ints["fan_speed"] == 3);

	// Still declared and still read where the box lacks it.
	source.caps.has_fan = 0;
	CHECK(settings::describe("fan_speed").ok());
	Result<std::string> read = settings::get("fan_speed");
	REQUIRE(read.ok());
	CHECK(read.value() == "3");
}

TEST_CASE("the parental lock holds its four rows while the box is locked, and the pin never", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource source;
	InstalledSystemSource installed(&source);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;

	const char *const held[] = { "parentallock_prompt", "parentallock_lockage",
	                             "parentallock_defaultlocked", "parentallock_zaptime" };
	source.parental_locked = true;
	for (size_t i = 0; i < 4; ++i)
	{
		INFO(held[i]);
		CHECK(settings::lockedNow(held[i]));
	}
	CHECK_FALSE(settings::lockedNow("parentallock_pincode"));
	CHECK_FALSE(settings::lockedNow("audio_AnalogMode"));

	Result<void> r = settings::set("parentallock_lockage", "16");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().status == Status::Conflict);
	CHECK(r.error().code == ErrorCode::SettingLocked);
	CHECK(store.ints.count("parentallock_lockage") == 0);
	// The lock answers ahead of the value: one the row would refuse anyway is refused for the lock.
	Result<void> bad = settings::check("parentallock_lockage", "not a number");
	REQUIRE_FALSE(bad.ok());
	CHECK(bad.error().code == ErrorCode::SettingLocked);
	REQUIRE(settings::set("parentallock_pincode", "1234").ok());

	source.parental_locked = false;
	for (size_t i = 0; i < 4; ++i)
		CHECK_FALSE(settings::lockedNow(held[i]));
	REQUIRE(settings::set("parentallock_lockage", "16").ok());

	// A lock that cannot be read never unlocks a row, and no box at all is one.
	source.parental_status = Status::Internal;
	CHECK(settings::lockedNow("parentallock_lockage"));
	source.parental_status = Status::Ok;
	setSystemSource(NULL);
	CHECK(settings::lockedNow("parentallock_lockage"));
	CHECK_FALSE(settings::lockedNow("parentallock_pincode"));
}
