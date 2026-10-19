/*
 * test_conditions.cpp - tests for the conditions a write of settings is held to
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

const Condition kSwitchOn[] = { when("t_switch").is(1) };

const Condition kEitherOn[] = { when("t_switch").is(1), when("t_other").is(1) };
const Condition kEither[] = { anyOf(kEitherOn) };

// Two settings each changeable only while the other is off.
const Condition kAOff[] = { when("t_a").is(0) };
const Condition kBOff[] = { when("t_b").is(0) };

const EnumValue kChoiceEntries[] =
{
	option(1).text("always"),
	option(2).text("with the switch").offeredWhen(kSwitchOn),
};

const Descriptor kRows[] =
{
	boolRow("t_switch")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	boolRow("t_other")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	intRow("t_dependent")
		.section("fixture")
		.label("label")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kSwitchOn)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	intRow("t_either")
		.section("fixture")
		.label("label")
		.range(0, 100)
		.defaultValue(0)
		.changeableWhen(kEither)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	boolRow("t_a")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.changeableWhen(kBOff)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	boolRow("t_b")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.changeableWhen(kAOff)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	enumRow("t_choice")
		.section("fixture")
		.label("label")
		.defaultValue(1)
		.values(kChoiceEntries)
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
};

struct Fixture
{
	InstalledSettingsTable table;
	FakeSettingsSource store;

	Fixture() : table(kRows, sizeof(kRows) / sizeof(kRows[0])) { setSettingsSource(&store); }
	~Fixture() { setSettingsSource(NULL); }
};

BatchOverlay overlay(const char *key, const char *value)
{
	BatchOverlay after;
	after.values.push_back(std::make_pair(std::string(key), std::string(value)));
	return after;
}

BatchMember member(const char *key, const char *value)
{
	BatchMember m;
	m.key = key;
	m.value = value;
	return m;
}

const BatchRefusal *refusalOf(const std::vector<BatchRefusal> &refused, const char *key)
{
	for (size_t i = 0; i < refused.size(); ++i)
	{
		if (refused[i].key == key)
			return &refused[i];
	}
	return NULL;
}

bool offers(const char *key, long value)
{
	Result<std::vector<SettingChoice> > listed = choices(key);
	if (!listed.ok())
		return false;
	for (size_t i = 0; i < listed.value().size(); ++i)
	{
		if (listed.value()[i].value == value)
			return true;
	}
	return false;
}

} // namespace

TEST_CASE("a setting whose conditions do not hold is refused and names what refuses it", "[settings][conditions]")
{
	Fixture f;

	Result<void> r = set("t_dependent", "5");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::SettingConditionNotMet);
	CHECK(r.error().status == Status::Conflict);
	REQUIRE(r.error().depends_on.size() == 1);
	CHECK(r.error().depends_on[0] == "t_switch");
	CHECK(r.error().message.find("t_switch") != std::string::npos);
	CHECK(f.store.ints.count("t_dependent") == 0);

	// Allowed once the setting it reads allows it.
	f.store.ints["t_switch"] = 1;
	REQUIRE(set("t_dependent", "5").ok());
	CHECK(f.store.ints["t_dependent"] == 5);
}

TEST_CASE("a group that does not hold names every setting it reads", "[settings][conditions]")
{
	Fixture f;

	Result<void> r = set("t_either", "5");
	REQUIRE_FALSE(r.ok());
	CHECK(r.error().code == ErrorCode::SettingConditionNotMet);
	REQUIRE(r.error().depends_on.size() == 2);
	CHECK(r.error().depends_on[0] == "t_switch");
	CHECK(r.error().depends_on[1] == "t_other");

	f.store.ints["t_other"] = 1;
	CHECK(set("t_either", "5").ok());
}

TEST_CASE("a value the setting already holds passes its conditions", "[settings][conditions]")
{
	Fixture f;

	// What the row holds, stored or as its default, is written back unasked.
	f.store.ints["t_dependent"] = 7;
	CHECK(set("t_dependent", "7").ok());
	f.store.ints.erase("t_dependent");
	CHECK(set("t_dependent", "0").ok());

	// Any other value is still refused.
	CHECK(set("t_dependent", "8").error().code == ErrorCode::SettingConditionNotMet);
	CHECK(settleOne("t_dependent", "8").error().code == ErrorCode::SettingConditionNotMet);
	CHECK(settleOne("t_dependent", "0").ok());
}

TEST_CASE("the conditions are judged on what the write leaves behind", "[settings][conditions]")
{
	Fixture f;

	const BatchOverlay on = overlay("t_switch", "1");
	CHECK(set("t_dependent", "5", &on).ok());
	CHECK(settleOne("t_dependent", "6", on.values).ok());

	// What the write leaves behind wins over what the store holds.
	f.store.ints["t_switch"] = 1;
	const BatchOverlay off = overlay("t_switch", "0");
	CHECK(set("t_dependent", "9", &off).error().code == ErrorCode::SettingConditionNotMet);
	CHECK(settleOne("t_dependent", "9", off.values).error().code == ErrorCode::SettingConditionNotMet);
}

TEST_CASE("an entry offered only while another setting allows it is read and written so", "[settings][conditions]")
{
	Fixture f;

	CHECK(offers("t_choice", 1));
	CHECK_FALSE(offers("t_choice", 2));

	Result<void> refused = set("t_choice", "2");
	REQUIRE_FALSE(refused.ok());
	CHECK(refused.error().code == ErrorCode::NotAListedValue);

	// Offered on what the write leaves behind.
	const BatchOverlay on = overlay("t_switch", "1");
	CHECK(set("t_choice", "2", &on).ok());

	// And with the store allowing it.
	f.store.ints["t_switch"] = 1;
	CHECK(offers("t_choice", 2));

	// The entry the row holds is the way back, even once it is no longer offered.
	f.store.ints["t_switch"] = 0;
	f.store.ints["t_choice"] = 2;
	CHECK_FALSE(offers("t_choice", 2));
	CHECK(set("t_choice", "2").ok());

	/* check() leaves the entry's own conditions to the settling, which has the batch. The
	   settling answers it as a condition that does not hold, as the source does: the value is
	   one the row lists, only not while the other setting is off. */
	f.store.ints["t_choice"] = 1;
	CHECK(check("t_choice", "2").ok());
	Result<void> settled = settleOne("t_choice", "2");
	REQUIRE_FALSE(settled.ok());
	CHECK(settled.error().code == ErrorCode::SettingConditionNotMet);
	CHECK(settled.error().depends_on.empty());
}

TEST_CASE("settings written together are judged together whatever their order", "[couple][conditions]")
{
	Fixture f;

	for (int order = 0; order < 2; ++order)
	{
		std::vector<BatchMember> batch;
		if (order == 0)
		{
			batch.push_back(member("t_dependent", "5"));
			batch.push_back(member("t_switch", "1"));
		}
		else
		{
			batch.push_back(member("t_switch", "1"));
			batch.push_back(member("t_dependent", "5"));
		}
		std::vector<BatchRefusal> refused;
		keepTogether(batch, refused);
		CHECK(refused.empty());
		CHECK(batch.size() == 2);
	}
}

TEST_CASE("a member whose conditions do not hold is refused alone and names what refuses it", "[couple][conditions]")
{
	Fixture f;

	std::vector<BatchMember> batch;
	batch.push_back(member("t_dependent", "5"));
	batch.push_back(member("t_other", "1"));
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	REQUIRE(refused.size() == 1);
	const BatchRefusal *no = refusalOf(refused, "t_dependent");
	REQUIRE(no != NULL);
	CHECK(no->error.code == ErrorCode::SettingConditionNotMet);
	REQUIRE(no->depends_on.size() == 1);
	CHECK(no->depends_on[0] == "t_switch");
	// The member that has nothing to do with it stays to be stored.
	REQUIRE(batch.size() == 1);
	CHECK(batch[0].key == "t_other");
}

TEST_CASE("a member refused by its own row does not allow another one", "[couple][conditions]")
{
	Fixture f;

	std::vector<BatchMember> batch;
	batch.push_back(member("t_switch", "seven"));
	batch.push_back(member("t_dependent", "5"));
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);

	// The switch never lands, so what it would have allowed is judged without it.
	CHECK(batch.empty());
	REQUIRE(refused.size() == 2);
	CHECK(refusalOf(refused, "t_switch")->error.code == ErrorCode::NotANumber);
	CHECK(refusalOf(refused, "t_dependent")->error.code == ErrorCode::SettingConditionNotMet);
}

TEST_CASE("a refusal by a condition is asked again until no answer changes", "[couple][conditions]")
{
	Fixture f;

	// The entry 2 of t_choice needs the switch; the switch is refused by nothing, the
	// dependent value needs it too. All three land together.
	std::vector<BatchMember> batch;
	batch.push_back(member("t_choice", "2"));
	batch.push_back(member("t_dependent", "5"));
	batch.push_back(member("t_switch", "1"));
	std::vector<BatchRefusal> refused;
	keepTogether(batch, refused);
	CHECK(refused.empty());
	CHECK(batch.size() == 3);

	// Without the switch neither lands, and each says so on its own.
	batch.clear();
	batch.push_back(member("t_choice", "2"));
	batch.push_back(member("t_dependent", "5"));
	refused.clear();
	keepTogether(batch, refused);
	CHECK(batch.empty());
	REQUIRE(refused.size() == 2);
	CHECK(refusalOf(refused, "t_choice")->error.code == ErrorCode::SettingConditionNotMet);
	CHECK(refusalOf(refused, "t_dependent")->error.code == ErrorCode::SettingConditionNotMet);
}

TEST_CASE("two settings whose new values refuse each other are both refused in either order", "[couple][conditions]")
{
	Fixture f;

	for (int order = 0; order < 2; ++order)
	{
		std::vector<BatchMember> batch;
		batch.push_back(member(order == 0 ? "t_a" : "t_b", "1"));
		batch.push_back(member(order == 0 ? "t_b" : "t_a", "1"));
		std::vector<BatchRefusal> refused;
		keepTogether(batch, refused);
		CHECK(batch.empty());
		REQUIRE(refused.size() == 2);
		CHECK(refusalOf(refused, "t_a")->depends_on[0] == "t_b");
		CHECK(refusalOf(refused, "t_b")->depends_on[0] == "t_a");
	}

	// Either alone is allowed.
	std::vector<BatchMember> one;
	one.push_back(member("t_a", "1"));
	std::vector<BatchRefusal> none;
	keepTogether(one, none);
	CHECK(none.empty());
	CHECK(one.size() == 1);
}

TEST_CASE("writeBatch writes what settles and answers what falls", "[settings][conditions]")
{
	Fixture f;

	std::vector<std::pair<std::string, std::string> > both;
	both.push_back(std::make_pair(std::string("t_dependent"), std::string("5")));
	both.push_back(std::make_pair(std::string("t_switch"), std::string("1")));
	Refusals failed;
	writeBatch(both, failed);
	CHECK(failed.empty());
	CHECK(f.store.ints["t_dependent"] == 5);
	CHECK(f.store.ints["t_switch"] == 1);

	// Alone and with the switch off, the dependent value is refused and nothing is written.
	f.store.ints["t_switch"] = 0;
	std::vector<std::pair<std::string, std::string> > alone;
	alone.push_back(std::make_pair(std::string("t_dependent"), std::string("6")));
	failed.clear();
	writeBatch(alone, failed);
	REQUIRE(failed.size() == 1);
	CHECK(failed[0].first == "t_dependent");
	CHECK(failed[0].second.code == ErrorCode::SettingConditionNotMet);
	CHECK(f.store.ints["t_dependent"] == 5);
}
