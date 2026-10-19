/*
 * test_couple.cpp - tests for the settings a write of several keeps together
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
#include "coreapi/settings/couple.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"
#include "support/fakes.h"
#include "support/settle.h"

#include <string>
#include <vector>

using namespace coreapi;
using namespace coreapi::settings;

namespace
{

const ChannelId kTv = 0x1e2404020001a001ULL;
const ChannelId kRadio = 0x1e2404020001b002ULL;
const ChannelId kHashed = 0x1e2404020001c003ULL;

ChannelInfo mk(ChannelId id, const char *name, ServiceKind kind)
{
	ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = kind;
	return c;
}

// A box with three channels and a store, both the fakes'.
struct Box
{
	FakeSettingsSource settings;
	FakeChannelSource channels;
	InstalledChannelSource in_channels;

	Box() : in_channels(&channels)
	{
		setSettingsSource(&settings);
		/* The start channels are changeable only while the box does not start on the last
		   channel, which is what it does by default. */
		settings.ints["uselastchannel"] = 0;
		channels.channels.push_back(mk(kTv, "First Channel Television HD", ServiceKind::Tv));
		channels.channels.push_back(mk(kRadio, "First Channel Radio Programme", ServiceKind::Radio));
		// A number sign is what no line of the settings file can hold.
		channels.channels.push_back(mk(kHashed, "Channel #3 Television", ServiceKind::Tv));
	}
	~Box() { setSettingsSource(NULL); }
};

std::string hex(ChannelId id)
{
	char buf[24];
	snprintf(buf, sizeof(buf), "%llx", (unsigned long long) id);
	return buf;
}

void add(std::vector<BatchMember> &batch, const char *key, const std::string &value)
{
	BatchMember m;
	m.key = key;
	m.value = value;
	batch.push_back(m);
}

const BatchMember *member(const std::vector<BatchMember> &batch, const char *key)
{
	for (size_t i = 0; i < batch.size(); ++i)
		if (batch[i].key == key)
			return &batch[i];
	return NULL;
}

const BatchRefusal *refusal(const std::vector<BatchRefusal> &refused, const char *key)
{
	for (size_t i = 0; i < refused.size(); ++i)
		if (refused[i].key == key)
			return &refused[i];
	return NULL;
}

} // anonymous namespace

TEST_CASE("the id of a start channel written alone brings the name of its channel", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", hex(kTv));
	add(batch, "startchannelradio_id", hex(kRadio));

	keepTogether(batch, refused);
	CHECK(refused.empty());
	REQUIRE(batch.size() == 4);
	const BatchMember *tv = member(batch, "startchanneltv");
	REQUIRE(tv != NULL);
	CHECK(tv->value == "First Channel Television HD");
	CHECK(tv->because == "startchanneltv_id");
	const BatchMember *radio = member(batch, "startchannelradio");
	REQUIRE(radio != NULL);
	CHECK(radio->value == "First Channel Radio Programme");
	CHECK(radio->because == "startchannelradio_id");
}

TEST_CASE("a start channel is refused while the box starts on the last channel", "[couple][conditions]")
{
	Box box;
	box.settings.ints["uselastchannel"] = 1;
	std::vector<BatchMember> batch;
	add(batch, "startchanneltv_id", hex(kTv));
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	// The id and what it brought are refused together, naming the setting that greys them out.
	CHECK(batch.empty());
	const BatchRefusal *id = refusal(refused, "startchanneltv_id");
	REQUIRE(id != NULL);
	CHECK(id->error.code == ErrorCode::SettingConditionNotMet);
	REQUIRE(id->depends_on.size() == 1);
	CHECK(id->depends_on[0] == "uselastchannel");

	// Written with the switch that allows it, both land.
	batch.clear();
	refused.clear();
	add(batch, "startchanneltv_id", hex(kTv));
	add(batch, "uselastchannel", "0");
	keepTogether(batch, refused);
	CHECK(refused.empty());
	CHECK(member(batch, "startchanneltv") != NULL);
}

TEST_CASE("a start channel's name written alone is refused as one fact with its id", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv", "Some Name");

	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].key == "startchanneltv");
	CHECK(refused[0].error.status == Status::Conflict);
	CHECK(refused[0].error.code == ErrorCode::SettingConditionNotMet);
	REQUIRE(refused[0].depends_on.size() == 1);
	CHECK(refused[0].depends_on[0] == "startchanneltv_id");
	CHECK(refused[0].error.message.find("startchanneltv_id") != std::string::npos);
}

TEST_CASE("both halves of a start channel are taken as sent", "[couple]")
{
	Box box;
	// Not a channel the list has, so a lookup would have refused it.
	box.channels.channels_status = Status::Internal;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv", "My Own Words");
	add(batch, "startchanneltv_id", hex(kTv));

	keepTogether(batch, refused);
	CHECK(refused.empty());
	REQUIRE(batch.size() == 2);
	CHECK(member(batch, "startchanneltv")->value == "My Own Words");
	CHECK(member(batch, "startchanneltv")->because.empty());
}

TEST_CASE("the start channel the box already starts on brings nothing and asks nobody", "[couple]")
{
	Box box;
	box.settings.strings["startchanneltv_id"] = hex(kTv);
	// A channel the list has lost since must not refuse writing it again.
	box.channels.channels_status = Status::Internal;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", hex(kTv));

	keepTogether(batch, refused);
	CHECK(refused.empty());
	REQUIRE(batch.size() == 1);
	CHECK(batch[0].key == "startchanneltv_id");
}

TEST_CASE("no channel at all empties the start channel's name", "[couple]")
{
	Box box;
	box.settings.strings["startchanneltv_id"] = hex(kTv);
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", "0");

	keepTogether(batch, refused);
	CHECK(refused.empty());
	const BatchMember *name = member(batch, "startchanneltv");
	REQUIRE(name != NULL);
	CHECK(name->value.empty());
}

TEST_CASE("an id of the other list or of no channel refuses the start channel", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", hex(kRadio));
	add(batch, "startchannelradio_id", hex(kTv));

	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 2);
	CHECK(refusal(refused, "startchanneltv_id")->error.code == ErrorCode::NotAListedValue);
	CHECK(refusal(refused, "startchanneltv_id")->depends_on.empty());
	CHECK(refusal(refused, "startchannelradio_id")->error.code == ErrorCode::NotAListedValue);

	std::vector<BatchMember> gone;
	std::vector<BatchRefusal> why;
	add(gone, "startchanneltv_id", "dead");
	keepTogether(gone, why);
	CHECK(gone.empty());
	REQUIRE(why.size() == 1);
	CHECK(why[0].error.code == ErrorCode::NoSuchChannel);
}

TEST_CASE("an id that is no identifier is the row's own refusal and brings nothing", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", "not an id");

	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].key == "startchanneltv_id");
	CHECK(refused[0].error.code == ErrorCode::BadString);
}

TEST_CASE("a name its row cannot hold refuses the id that brought it", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", hex(kHashed));

	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].key == "startchanneltv_id");
	CHECK(refused[0].error.code == ErrorCode::BadString);
}

/* A name refused before the pairs are looked at never reaches them, so the id is an id
   written alone: it lands and brings the name of its channel, as in the source. The pair the
   box keeps is the channel's, and the refused name stays refused. */
TEST_CASE("a name refused before the pairs are looked at leaves the id to bring its own", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "startchanneltv_id", hex(kTv));
	BatchRefusal before;
	before.key = "startchanneltv";
	before.error = Error(Status::InvalidArgument, ErrorCode::BadString, "refused by the caller");
	refused.push_back(before);

	keepTogether(batch, refused);
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].key == "startchanneltv");
	const BatchMember *name = member(batch, "startchanneltv");
	REQUIRE(name != NULL);
	CHECK(name->value == "First Channel Television HD");
	CHECK(name->because == "startchanneltv_id");
}

TEST_CASE("the city and the location of the weather are written together or not at all", "[couple]")
{
	Box box;
	const char *const halves[][2] = {
		{ "weather_city", "weather_location" },
		{ "weather_location", "weather_city" }
	};
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(halves[i][0]);
		std::vector<BatchMember> batch;
		std::vector<BatchRefusal> refused;
		add(batch, halves[i][0], "Hamburg");
		keepTogether(batch, refused);
		CHECK(batch.empty());
		REQUIRE(refused.size() == 1);
		CHECK(refused[0].error.code == ErrorCode::SettingConditionNotMet);
		REQUIRE(refused[0].depends_on.size() == 1);
		CHECK(refused[0].depends_on[0] == halves[i][1]);
	}

	// Together, and the postal code a search had entered goes with the place.
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "weather_city", "Hamburg");
	add(batch, "weather_location", "53.55,9.99");
	keepTogether(batch, refused);
	CHECK(refused.empty());
	REQUIRE(batch.size() == 3);
	const BatchMember *postal = member(batch, "weather_postalcode");
	REQUIRE(postal != NULL);
	CHECK(postal->value.empty());
	CHECK(postal->because == "weather_city");
}

TEST_CASE("the place written back unchanged passes while the weather is off", "[couple][conditions]")
{
	Box box;
	box.settings.ints["weather_enabled"] = 0;
	box.settings.strings["weather_city"] = "Berlin";
	box.settings.strings["weather_location"] = "52.52,13.40";
	box.settings.strings["weather_postalcode"] = "10178";

	// What a reader sent back as it read it changes nothing and brings nothing.
	std::vector<BatchMember> batch;
	add(batch, "weather_city", "Berlin");
	add(batch, "weather_location", "52.52,13.40");
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);
	CHECK(refused.empty());
	CHECK(batch.size() == 2);
	CHECK(member(batch, "weather_postalcode") == NULL);

	// Another place is a change, which the weather being off refuses, naming it.
	batch.clear();
	refused.clear();
	add(batch, "weather_city", "Hamburg");
	add(batch, "weather_location", "53.55,9.99");
	keepTogether(batch, refused);
	CHECK(batch.empty());
	const BatchRefusal *city = refusal(refused, "weather_city");
	REQUIRE(city != NULL);
	CHECK(city->error.code == ErrorCode::SettingConditionNotMet);
	REQUIRE(city->depends_on.size() == 1);
	CHECK(city->depends_on[0] == "weather_enabled");
	CHECK(refusal(refused, "weather_location") != NULL);
}

TEST_CASE("a postal code with text contradicts the place written with it", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "weather_city", "Hamburg");
	add(batch, "weather_location", "53.55,9.99");
	add(batch, "weather_postalcode", "20095");

	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 3);
	CHECK(refusal(refused, "weather_postalcode")->depends_on[0] == "weather_city");
	CHECK(refusal(refused, "weather_city")->depends_on[0] == "weather_postalcode");
	const BatchRefusal *location = refusal(refused, "weather_location");
	REQUIRE(location != NULL);
	CHECK(location->error.code == ErrorCode::SettingConditionNotMet);
	CHECK(location->depends_on[0] == "weather_city");

	// An empty one is the same thing the place brings and is taken as sent.
	std::vector<BatchMember> same;
	std::vector<BatchRefusal> none;
	add(same, "weather_city", "Hamburg");
	add(same, "weather_location", "53.55,9.99");
	add(same, "weather_postalcode", "");
	keepTogether(same, none);
	CHECK(none.empty());
	REQUIRE(same.size() == 3);
	CHECK(member(same, "weather_postalcode")->because.empty());
}

TEST_CASE("a half its row refuses takes the other half and what it brought down", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "weather_city", "Ham\nburg");
	add(batch, "weather_location", "53.55,9.99");

	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 2);
	CHECK(refusal(refused, "weather_city")->error.code == ErrorCode::BadString);
	const BatchRefusal *location = refusal(refused, "weather_location");
	REQUIRE(location != NULL);
	CHECK(location->error.code == ErrorCode::SettingConditionNotMet);
	CHECK(location->depends_on[0] == "weather_city");
}

TEST_CASE("settings in no pair are held to their rows and to nothing else", "[couple]")
{
	Box box;
	std::vector<BatchMember> batch;
	std::vector<BatchRefusal> refused;
	add(batch, "weather_postalcode", "20095");
	add(batch, "audio_AnalogMode", "not a number");
	add(batch, "start_volume", "1");

	keepTogether(batch, refused);
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].key == "audio_AnalogMode");
	CHECK(refused[0].error.code == ErrorCode::NotANumber);
	REQUIRE(batch.size() == 2);
	CHECK(member(batch, "weather_postalcode")->value == "20095");
	CHECK(member(batch, "start_volume") != NULL);
}

TEST_CASE("the pairs name each other and the start channels their list", "[couple]")
{
	const char *writes = NULL;
	CHECK(std::string(pairPartner("startchanneltv", &writes)) == "startchanneltv_id");
	CHECK(std::string(writes) == "id");
	CHECK(std::string(pairPartner("startchannelradio_id", &writes)) == "startchannelradio");
	CHECK(std::string(writes) == "id");
	CHECK(std::string(pairPartner("weather_location", &writes)) == "weather_city");
	CHECK(std::string(writes) == "both");
	CHECK(pairPartner("weather_postalcode") == NULL);
	CHECK(pairPartner("language") == NULL);

	CHECK(std::string(startChannelKind("startchanneltv_id")) == "tv");
	CHECK(std::string(startChannelKind("startchannelradio")) == "radio");
	CHECK(startChannelKind("weather_city") == NULL);

	// Every key a pair names is a row the box declares.
	const char *const keys[] = { "startchanneltv", "startchanneltv_id", "startchannelradio",
		"startchannelradio_id", "weather_city", "weather_location", "weather_postalcode" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		INFO(keys[i]);
		CHECK(describe(keys[i]).ok());
	}
}

// The ceiling the two brightness rows state, which every write path is held to.
TEST_CASE("the Pearl panel takes brightness up to seven and every other panel up to ten", "[couple][lcd4l]")
{
	CHECK(settings::lcd4lBrightnessCeiling(0) == 7);
	CHECK(settings::lcd4lBrightnessCeiling(1) == 10);
	CHECK(settings::lcd4lBrightnessCeiling(2) == 10);
	CHECK(settings::lcd4lBrightnessCeiling(3) == 10);
	// The VU+ Solo 4K panel and the three PNG outputs take ten as well.
	CHECK(settings::lcd4lBrightnessCeiling(4) == 10);
	CHECK(settings::lcd4lBrightnessCeiling(5) == 10);
	CHECK(settings::lcd4lBrightnessCeiling(6) == 10);
	CHECK(settings::lcd4lBrightnessCeiling(7) == 10);
	CHECK(settings::lcd4lBrightnessCeiling(8) == 7);
	// A type nobody knows gets the lower ceiling, as the screen gave it.
	CHECK(settings::lcd4lBrightnessCeiling(9) == 7);
}

namespace
{

/* The three LCD4Linux rows the coupling reads, under their own keys: the unit build has no
   LCD4Linux, so the box's own rows are not declared here. */
// As the box's row: the standby brightness is greyed out while standby is the box switched off.
const Condition kNotRealShutdown[] = { when("shutdown_real").is(0) };

const EnumValue kPanels[] =
{
	option(0).text("320x240 Pearl DPF"),
	option(1).text("800x480 Samsung SPF")
};

const Descriptor kLcd4lRows[] =
{
	enumRow("lcd4l_display_type")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.values(kPanels)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	intRow("lcd4l_brightness")
		.section("fixture")
		.label("label")
		.range(1, 10)
		.defaultValue(5)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	intRow("lcd4l_brightness_standby")
		.section("fixture")
		.label("label")
		.range(1, 10)
		.defaultValue(3)
		.changeableWhen(kNotRealShutdown)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	boolRow("shutdown_real")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
};

struct Lcd4lBox
{
	InstalledSettingsTable table;
	FakeSettingsSource settings;

	Lcd4lBox() : table(kLcd4lRows, sizeof(kLcd4lRows) / sizeof(kLcd4lRows[0])) { setSettingsSource(&settings); }
	~Lcd4lBox() { setSettingsSource(NULL); }
};

} // anonymous namespace

TEST_CASE("a write of the LCD4Linux panel brings both brightnesses inside it", "[couple][lcd4l]")
{
	Lcd4lBox box;
	box.settings.ints["lcd4l_display_type"] = 1;
	box.settings.ints["lcd4l_brightness"] = 9;
	box.settings.ints["lcd4l_brightness_standby"] = 8;

	std::vector<BatchMember> batch;
	add(batch, "lcd4l_display_type", "0");
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	CHECK(refused.empty());
	const BatchMember *on = member(batch, "lcd4l_brightness");
	REQUIRE(on != NULL);
	CHECK(on->value == "7");
	CHECK(on->because == "lcd4l_display_type");
	const BatchMember *standby = member(batch, "lcd4l_brightness_standby");
	REQUIRE(standby != NULL);
	CHECK(standby->value == "7");
}

TEST_CASE("a brightness inside the new panel stays as it is", "[couple][lcd4l]")
{
	Lcd4lBox box;
	box.settings.ints["lcd4l_display_type"] = 0;
	box.settings.ints["lcd4l_brightness"] = 5;
	box.settings.ints["lcd4l_brightness_standby"] = 3;

	std::vector<BatchMember> batch;
	add(batch, "lcd4l_display_type", "1");
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	CHECK(refused.empty());
	REQUIRE(batch.size() == 1);
	CHECK(batch[0].key == "lcd4l_display_type");
}

TEST_CASE("a brightness the panel does not take is refused, naming the panel", "[couple][lcd4l]")
{
	Lcd4lBox box;
	box.settings.ints["lcd4l_display_type"] = 0;

	std::vector<BatchMember> batch;
	add(batch, "lcd4l_brightness", "9");
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	CHECK(batch.empty());
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].key == "lcd4l_brightness");
	CHECK(refused[0].error.code == ErrorCode::SettingConditionNotMet);
	REQUIRE(refused[0].depends_on.size() == 1);
	CHECK(refused[0].depends_on[0] == "lcd4l_display_type");

	// With the panel that takes it, in the same write, both land.
	batch.clear();
	refused.clear();
	add(batch, "lcd4l_brightness", "9");
	add(batch, "lcd4l_display_type", "1");
	keepTogether(batch, refused);
	CHECK(refused.empty());
	CHECK(batch.size() == 2);
}

TEST_CASE("a brightness above the new panel written with it contradicts it", "[couple][lcd4l]")
{
	Lcd4lBox box;
	box.settings.ints["lcd4l_display_type"] = 1;

	std::vector<BatchMember> batch;
	add(batch, "lcd4l_display_type", "0");
	add(batch, "lcd4l_brightness", "9");
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	// The panel would bring it to seven, and the write names nine: both are refused.
	CHECK(batch.empty());
	CHECK(refusal(refused, "lcd4l_display_type") != NULL);
	CHECK(refusal(refused, "lcd4l_brightness") != NULL);
}

TEST_CASE("a new panel brings the standby brightness down while standby is switched off", "[couple][lcd4l]")
{
	Lcd4lBox box;
	box.settings.ints["lcd4l_display_type"] = 1;
	box.settings.ints["lcd4l_brightness"] = 5;
	box.settings.ints["lcd4l_brightness_standby"] = 9;
	box.settings.ints["shutdown_real"] = 1;

	// The standby brightness is greyed out, and what the panel brings is no pick: both land.
	std::vector<BatchMember> batch;
	add(batch, "lcd4l_display_type", "0");
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);
	CHECK(refused.empty());
	CHECK(member(batch, "lcd4l_display_type") != NULL);
	const BatchMember *standby = member(batch, "lcd4l_brightness_standby");
	REQUIRE(standby != NULL);
	CHECK(standby->value == "7");

	// Named by the caller, the greyed standby brightness is still refused.
	batch.clear();
	refused.clear();
	add(batch, "lcd4l_brightness_standby", "4");
	keepTogether(batch, refused);
	REQUIRE(refused.size() == 1);
	CHECK(refused[0].error.code == ErrorCode::SettingConditionNotMet);
	CHECK(refused[0].depends_on[0] == "shutdown_real");

	// Written, as the route writes: the panel and the brightness it brings both land.
	std::vector<std::pair<std::string, std::string> > members;
	members.push_back(std::make_pair(std::string("lcd4l_display_type"), std::string("0")));
	Refusals failed;
	writeBatch(members, failed);
	CHECK(failed.empty());
	CHECK(box.settings.ints["lcd4l_display_type"] == 0);
	CHECK(box.settings.ints["lcd4l_brightness_standby"] == 7);
}
