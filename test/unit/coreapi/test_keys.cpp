/*
 * test_keys.cpp - tests for the key settings
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (adaptation)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/catch.hpp"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"

#include <stdint.h>
#include <stdio.h>

#include <linux/input.h>

#include <driver/rcinput.h>

using namespace coreapi;

namespace
{

long unnamedCode()
{
	for (long c = 1; c <= (long) KEY_MAX; ++c)
		if (keySource().name(c).empty())
			return c;
	return 0;
}

std::string decimal0(long v)
{
	char out[32];
	snprintf(out, sizeof(out), "%ld", v);
	return std::string(out);
}

} // anonymous namespace

TEST_CASE("a key row is sane between bounds that hold its default and names no words", "[keys]")
{
	Descriptor d = { "k", ValueType::Key, "s", "l", NULL, -2, 100, NULL, 0, 5, NULL, false, false,
		COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	REQUIRE(descriptorIsSane(d));

	d.default_int = 101;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = -3;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 5;

	d.min = 101;
	d.max = 100;
	REQUIRE_FALSE(descriptorIsSane(d));
	d.min = -2;

	const EnumValue words[] = { { -2, "a", NULL, NULL, NULL, 0 } };
	d.values = words;
	d.value_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("the key source takes every code a remote can send and names the keys it knows", "[keys]")
{
	installRealKeySource();
	struct Restore { ~Restore() { setKeySource(0); } } restore;

	const KeySource &src = keySource();
	const long none = (int32_t) CRCInput::RC_nokey;
	CHECK(src.known(none));
	CHECK(src.known((long) CRCInput::RC_ok));
	CHECK(src.known((long) CRCInput::RC_ok | (long) CRCInput::RC_Repeat));
	CHECK(src.name((long) CRCInput::RC_ok) == "ok");
	CHECK(src.name((long) CRCInput::RC_ok | (long) CRCInput::RC_Repeat) == "ok (long)");
	CHECK(src.name(none) == "none");

	// Neither a release nor a code past the table is a key.
	CHECK_FALSE(src.known((long) CRCInput::RC_ok | (long) CRCInput::RC_Release));
	CHECK_FALSE(src.known((long) CRCInput::RC_MaxRC + 1));
	CHECK_FALSE(src.known((long) KEY_MAX + 1));
	CHECK_FALSE(src.known((long) (KEY_MAX + 1) | (long) CRCInput::RC_Repeat));
	CHECK_FALSE(src.known(0));
	CHECK_FALSE(src.known(-1));
	CHECK(src.known((long) KEY_MAX));
	// A code with no name is still a key the chooser can store, plain or held.
	const long unnamed = unnamedCode();
	REQUIRE(unnamed != 0);
	CHECK(src.known(unnamed));
	CHECK(src.known(unnamed | (long) CRCInput::RC_Repeat));
	CHECK(src.name(unnamed).empty());
	CHECK(src.name(unnamed | (long) CRCInput::RC_Repeat).empty());

	const std::vector<KeyName> all = src.all();
	REQUIRE(all.size() > 100);
	CHECK(all[0].code == none);
	size_t held = 0;
	for (size_t i = 0; i < all.size(); ++i)
	{
		CHECK(src.known(all[i].code));
		CHECK_FALSE(all[i].name.empty());
		if (all[i].code > 0 && (all[i].code & (long) CRCInput::RC_Repeat) != 0)
			++held;
		for (size_t j = 0; j < i; ++j)
			REQUIRE(all[j].code != all[i].code);
	}
	// Every named key once plain and once held.
	CHECK(held * 2 == all.size() - 1);
}

TEST_CASE("every key row starts on a key the input layer can deliver", "[keys]")
{
	installRealKeySource();
	struct Restore { ~Restore() { setKeySource(0); } } restore;

	const Descriptor *t = settingsTable();
	size_t keys = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		if (t[i].type != ValueType::Key)
			continue;
		++keys;
		INFO(t[i].key);
		CHECK(keySource().known(t[i].default_int));
		CHECK(t[i].field.read_number != NULL);
	}
	// The rows the keybinding screen offers a key chooser for; the six picture in
	// picture keys are there only where that is built.
#if ENABLE_PIP
	CHECK(keys == 56);
#else
	CHECK(keys == 50);
#endif
}

TEST_CASE("a key setting takes any code a remote can send and refuses the rest between its bounds", "[keys]")
{
	FakeSettingsSource f;
	setSettingsSource(&f);
	installRealKeySource();
	struct Restore { ~Restore() { setKeySource(0); setSettingsSource(NULL); } } restore;

	REQUIRE(settings::set("key_power_off", decimal0((long) CRCInput::RC_home)).ok());
	CHECK(f.ints["key_power_off"] == (long) CRCInput::RC_home);

	// Held, and no key at all, are keys.
	REQUIRE(settings::set("key_power_off", decimal0((long) CRCInput::RC_ok | (long) CRCInput::RC_Repeat)).ok());
	REQUIRE(settings::set("key_power_off", decimal0((int32_t) CRCInput::RC_nokey)).ok());
	CHECK(f.ints["key_power_off"] == (long) (int32_t) CRCInput::RC_nokey);

	// A key with no name is stored as it is: the chooser stores it too.
	const long unnamed = unnamedCode();
	REQUIRE(settings::set("key_power_off", decimal0(unnamed)).ok());
	CHECK(f.ints["key_power_off"] == unnamed);

	// A release is between the bounds and is no key; past the bounds is out of range.
	Result<void> release = settings::set("key_power_off",
		decimal0((long) CRCInput::RC_ok | (long) CRCInput::RC_Release));
	REQUIRE_FALSE(release.ok());
	CHECK(release.error().code == ErrorCode::NotAListedValue);
	Result<void> past = settings::set("key_power_off", decimal0((long) CRCInput::RC_MaxRC + 1));
	REQUIRE_FALSE(past.ok());
	CHECK(past.error().code == ErrorCode::OutOfRange);
	CHECK(f.ints["key_power_off"] == unnamed);
}
