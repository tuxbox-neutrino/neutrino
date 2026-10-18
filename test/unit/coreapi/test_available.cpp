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

#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

namespace
{

/* What a setting offers on this box, in the order it offers it: each value with
   the catalog name it is offered under, or with its own words where it has no
   such name. The words are the ones its screen shows, and four of these screens
   are ones no scan can read, so these cases are what holds the pairing. */
std::string offeredValues(const char *key)
{
	Result<std::vector<SettingChoice> > r = settings::choices(key);
	if (!r.ok())
		return "(none)";
	std::string t;
	for (size_t i = 0; i < r.value().size(); ++i)
	{
		const SettingChoice &c = r.value()[i];
		char n[24];
		snprintf(n, sizeof(n), "%ld", c.value);
		t += (i > 0 ? "," : "") + std::string(n) + "=" + (c.label_key.empty() ? c.label : c.label_key);
	}
	return t;
}

// A write the box turns down for the value, leaving the store as it was.
void refusedAsUnlisted(FakeSettingsSource &store, const char *key, const char *value)
{
	INFO(key << " = " << value);
	const size_t before = store.ints.count(key);
	const long was = before ? store.ints[key] : 0;
	Result<void> r = settings::set(key, value);
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotAListedValue);
	CHECK(store.ints.count(key) == before);
	if (before)
		CHECK(store.ints[key] == was);
}

} // namespace

/* Entries only some boxes have. Each is offered where the box has what it needs
   and turned down on a write where it does not, as the screen leaves it out; the
   rest of the choice stays as it is. */
TEST_CASE("the EPG scan modes that scan live need a second tuner switched on", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;
	REQUIRE(settings::describe("epg_scan_mode").ok());

	// No tuner source at all is a box that cannot say, which offers no second tuner.
	CHECK(offeredValues("epg_scan_mode") == "0=options.off,2=miscsettings.epg_scan_standby");

	FakeTunerSource tuners;
	InstalledTunerSource installed_tuners(&tuners);
	tuners.enabled = 1;
	CHECK(offeredValues("epg_scan_mode") == "0=options.off,2=miscsettings.epg_scan_standby");
	refusedAsUnlisted(store, "epg_scan_mode", "1");
	refusedAsUnlisted(store, "epg_scan_mode", "3");
	REQUIRE(settings::set("epg_scan_mode", "2").ok());
	CHECK(store.ints["epg_scan_mode"] == 2);

	tuners.enabled = 2;
	CHECK(offeredValues("epg_scan_mode") ==
	      "0=options.off,2=miscsettings.epg_scan_standby,"
	      "1=miscsettings.epg_scan_live,3=miscsettings.epg_scan_always");
	REQUIRE(settings::set("epg_scan_mode", "3").ok());
	CHECK(store.ints["epg_scan_mode"] == 3);

	// A count the tuners could not give is a box that cannot say, whatever it wrote.
	tuners.enabled_status = Status::Internal;
	CHECK(offeredValues("epg_scan_mode") == "0=options.off,2=miscsettings.epg_scan_standby");
}

TEST_CASE("the disk format offers the file systems the box can write", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;
	REQUIRE(settings::describe("hdd_fs").ok());

	box.format_tools.push_back("ext4");
	box.format_tools.push_back("vfat");
	CHECK(offeredValues("hdd_fs") == "0=ext4,4=vfat");
	refusedAsUnlisted(store, "hdd_fs", "1");
	REQUIRE(settings::set("hdd_fs", "4").ok());
	CHECK(store.ints["hdd_fs"] == 4);

	// Every tool the table names, in the order of the disk menu's own table.
	box.format_tools.clear();
	const char *const all[] = { "xfs", "exfat", "vfat", "f2fs", "ext2", "ext3", "ext4" };
	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
		box.format_tools.push_back(all[i]);
	CHECK(offeredValues("hdd_fs") == "0=ext4,1=ext3,2=ext2,3=f2fs,4=vfat,5=exfat,6=xfs");

	// A box that can format nothing offers nothing, and takes nothing but the value
	// it holds and the default, which are no new pick.
	box.format_tools.clear();
	CHECK(offeredValues("hdd_fs") == "(none)");
	refusedAsUnlisted(store, "hdd_fs", "1");
	REQUIRE(settings::set("hdd_fs", "4").ok());
	REQUIRE(settings::set("hdd_fs", "0").ok());

	box.format_tools.push_back("ext4");
	box.format_tools_status = Status::Internal;
	CHECK(offeredValues("hdd_fs") == "(none)");
}

TEST_CASE("the OSD size offers the sizes the box draws its screen at", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeOsdResolution osd;
	InstalledOsdResolution installed_osd(&osd);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;
	REQUIRE(settings::describe("osd_resolution").ok());

	CHECK(offeredValues("osd_resolution") == "0=1280x720,1=1920x1080");

	osd.sizes.clear();
	osd.sizes.push_back(std::make_pair(1280, 720));
	CHECK(offeredValues("osd_resolution") == "0=1280x720");
	Result<void> r = settings::set("osd_resolution", "1");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::NotAListedValue);
	CHECK(osd.writes == 0);

	osd.sizes_status = Status::NotSupported;
	CHECK(offeredValues("osd_resolution") == "(none)");
}

TEST_CASE("the 4:3 mode and the picture format offer 14:9 where the box shows it", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;
	REQUIRE(settings::describe("video_43mode").ok());
	REQUIRE(settings::describe("video_Format").ok());

	box.caps.can_ps_14_9 = 0;
	box.caps.can_ar_14_9 = 0;
	CHECK(offeredValues("video_43mode") == "0=videomenu.panscan,1=videomenu.letterbox,2=videomenu.fullscreen");
	CHECK(offeredValues("video_Format") == "1=videomenu.videoformat_43,3=videomenu.videoformat_169");
	refusedAsUnlisted(store, "video_43mode", "3");
	refusedAsUnlisted(store, "video_Format", "2");

	box.caps.can_ps_14_9 = 1;
	CHECK(offeredValues("video_43mode") ==
	      "0=videomenu.panscan,3=videomenu.panscan2,1=videomenu.letterbox,2=videomenu.fullscreen");
	CHECK(offeredValues("video_Format") == "1=videomenu.videoformat_43,3=videomenu.videoformat_169");
	REQUIRE(settings::set("video_43mode", "3").ok());
	CHECK(store.ints["video_43mode"] == 3);

	box.caps.can_ar_14_9 = 1;
	CHECK(offeredValues("video_Format") ==
	      "1=videomenu.videoformat_43,2=videomenu.videoformat_149,3=videomenu.videoformat_169");
	REQUIRE(settings::set("video_Format", "2").ok());
	CHECK(store.ints["video_Format"] == 2);
}

// Decided by the hardware family the build is for, and asked of nothing at run time.
TEST_CASE("the web stream size offers the larger two where the build decodes them", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;
	REQUIRE(settings::describe("livestreamResolution").ok());

#if HAVE_CST_HARDWARE
	CHECK(offeredValues("livestreamResolution") == "1920=1920x1080,1280=1280x720,854=854x480,640=640x360,480=480x270");
	refusedAsUnlisted(store, "livestreamResolution", "3840");
#else
	CHECK(offeredValues("livestreamResolution") ==
	      "3840=3840x2160,2560=2560x1440,1920=1920x1080,1280=1280x720,854=854x480,640=640x360,480=480x270");
	REQUIRE(settings::set("livestreamResolution", "3840").ok());
	CHECK(store.ints["livestreamResolution"] == 3840);
#endif
	refusedAsUnlisted(store, "livestreamResolution", "1000");
}

/* A number whose list the box supplies: the tuners the module can be bound to are the
   ones the box has, and a write is held to them. The default and the value stored are
   the way back and pass whatever the list says. */
TEST_CASE("the module's tuner is one of the tuners the box has", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;
	REQUIRE(settings::describe("ci_tuner").ok());

	// No tuner source is a box that cannot say: no list, and the range alone holds.
	CHECK(offeredValues("ci_tuner") == "(none)");
	REQUIRE(settings::set("ci_tuner", "5").ok());

	FakeTunerSource tuners;
	InstalledTunerSource installed_tuners(&tuners);
	FrontendInfo first;
	first.number = 0;
	first.name = "Si2166";
	tuners.tuners.push_back(first);
	FrontendInfo second;
	second.number = 1;
	second.name = "Si2169";
	tuners.tuners.push_back(second);
	CHECK(offeredValues("ci_tuner") == "-1=options.off,0=1: Si2166,1=2: Si2169");

	REQUIRE(settings::set("ci_tuner", "1").ok());
	CHECK(store.ints["ci_tuner"] == 1);
	refusedAsUnlisted(store, "ci_tuner", "2");
	REQUIRE(settings::set("ci_tuner", "-1").ok());
	store.ints["ci_tuner"] = 5;
	REQUIRE(settings::set("ci_tuner", "5").ok());
	refusedAsUnlisted(store, "ci_tuner", "3");

	// A list the tuners cannot give is a box that cannot say again.
	tuners.list_status = Status::Internal;
	CHECK(offeredValues("ci_tuner") == "(none)");
	REQUIRE(settings::set("ci_tuner", "3").ok());
}

/* A value a box holds and a choice no longer offers, for an entry the box lacks now or
   a number a range left behind before the setting became a choice: writing it back is
   no new pick, so it passes, as the default does. Anything else not offered is refused,
   so a client may send back what it read and nothing more. */
TEST_CASE("a choice takes back the value it holds and its default, whatever it offers now", "[available]")
{
	FakeSettingsSource store;
	setSettingsSource(&store);
	FakeSystemSource box;
	InstalledSystemSource installed(&box);
	FakeTunerSource tuners;
	InstalledTunerSource installed_tuners(&tuners);
	struct Restore { ~Restore() { setSettingsSource(NULL); } } restore;

	// A live mode, stored while a second tuner was switched on.
	tuners.enabled = 1;
	store.ints["epg_scan_mode"] = 1;
	CHECK(offeredValues("epg_scan_mode") == "0=options.off,2=miscsettings.epg_scan_standby");
	REQUIRE(settings::set("epg_scan_mode", "1").ok());
	refusedAsUnlisted(store, "epg_scan_mode", "3");
	REQUIRE(settings::set("epg_scan_mode", "2").ok());
	CHECK(store.ints["epg_scan_mode"] == 2);
	// Once something else is stored, the old value is just one the choice lacks.
	refusedAsUnlisted(store, "epg_scan_mode", "1");

	// A number no list of it ever had, left by the range the setting had before.
	store.ints["sleeptimer_min"] = 45;
	REQUIRE(settings::set("sleeptimer_min", "45").ok());
	refusedAsUnlisted(store, "sleeptimer_min", "50");
	REQUIRE(settings::set("sleeptimer_min", "0").ok());
	CHECK(store.ints["sleeptimer_min"] == 0);
}
