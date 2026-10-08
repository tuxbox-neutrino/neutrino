/*
 * test_settingstable.cpp - tests for the settings table
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

#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"
#include "coreapi/settings/settingstable.h"
#include "support/fakes.h"

#include <cstdio>
#include <map>
#include <string>

/* The table held to its own rules, which is the part no reading of the program's source
   can answer. Whether a row says what the program says is checked beside this; whether
   the rows make a table is checked here.

   A key declared twice and a row that is not sane are already cases of their own. What
   is left is the one thing a row says about another row: a condition naming a key, which
   is a pointer to a row written as text and checked by nothing at all. Declared against
   a key that was renamed, the condition holds every time, so the setting is shown where
   it does not apply and no run ever says so. */

#include "support/counts.h"

using namespace coreapi;

namespace
{


// A row of every kind the check has to hold, so that what it walks does not
// depend on which kinds the shipped table happens to carry.
const Condition kOnRow[] =
{
	{ "fixture_flag", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 }
};

const Condition kOnNothing[] =
{
	{ "fixture_no_such_key", CompareOp::Ne, 0, NULL, 0, NULL, NULL, 0 }
};

const Descriptor kSound[] =
{
	{
		"fixture_flag", ValueType::Bool, "fixture", "fixture.flag", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_gated", ValueType::Int, "fixture", "fixture.gated", NULL,
		0, 9, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kOnRow), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	}
};

const Descriptor kDangling[] =
{
	{
		"fixture_flag", ValueType::Bool, "fixture", "fixture.flag", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	{
		"fixture_gated", ValueType::Int, "fixture", "fixture.gated", NULL,
		0, 9, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kOnNothing), COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	}
};

// Every condition of the installed table that names a key the table declares,
// and how many were looked at. Written once and driven over the fixtures as
// well as over the shipped table, so what the case below asserts is what it
// proves it can refuse.
size_t danglingConditions(size_t &looked)
{
	size_t bad = 0;
	looked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		for (size_t c = 0; c < d.condition_count; ++c)
		{
			++looked;
			if (!settings::describe(d.conditions[c].key).ok())
				++bad;
		}
	}
	return bad;
}

} // namespace

/* The check proves itself over a table written to fail it before it is asked
   about the program's, because a table that declared no condition at all would
   otherwise pass this by looking at nothing, and that is exactly the shape a
   check has when it has stopped working. */
TEST_CASE("a condition naming a key nothing declares is caught", "[settingstable]")
{
	size_t looked = 0;

	{
		InstalledSettingsTable sound(kSound, sizeof(kSound) / sizeof(kSound[0]));
		REQUIRE(danglingConditions(looked) == 0);
		REQUIRE(looked == 1);
	}

	{
		InstalledSettingsTable dangling(kDangling, sizeof(kDangling) / sizeof(kDangling[0]));
		REQUIRE(danglingConditions(looked) == 1);
		REQUIRE(looked == 1);
	}
}

TEST_CASE("every condition the program declares names a key it declares", "[settingstable]")
{
	REQUIRE(settingsTableCount() > 0);

	size_t looked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		for (size_t c = 0; c < d.condition_count; ++c)
		{
			++looked;
			INFO("row " << d.key << " is shown when " << d.conditions[c].key << " holds");
			CHECK(settings::describe(d.conditions[c].key).ok());
		}
	}

	/* The count and not merely the comparison: a condition is carried by the
	   row it gates, so a section that stopped being linked takes its conditions
	   with it and this case goes on passing over the ones that remain. The case
	   above is what says a bad condition would be refused. */
	INFO("conditions checked: " << looked);
	recordCount("conditions checked", looked);
}

/* The declared side alone, split by section, which is the unit the remaining
   work is done in. The breakdown is reported and the total is not: it is the
   denominator every other count here is a fraction of, and a fraction of a
   number nothing holds says nothing. */
TEST_CASE("the declared count is what this build links", "[settingstable]")
{
	REQUIRE(settingsTableCount() > 0);
	recordCount("rows this build links", settingsTableCount());

	std::map<std::string, size_t> bySection;
	for (size_t i = 0; i < settingsTableCount(); ++i)
		bySection[settingsTable()[i].section]++;

	std::string breakdown;
	for (std::map<std::string, size_t>::const_iterator it = bySection.begin();
	     it != bySection.end(); ++it)
	{
		char n[32];
		snprintf(n, sizeof(n), "%u", (unsigned) it->second);
		breakdown += (breakdown.empty() ? "" : ", ");
		breakdown += it->first + " " + n;
	}

	INFO("settings declared: " << settingsTableCount()
	     << " in " << bySection.size() << " sections: " << breakdown);
	CHECK(bySection.size() > 0);
}

/* The other half of the coverage, which is what the tables leave out. The
   arithmetic against the settings struct is a check over the source and cannot
   be run from here; what can be is that the list is a list of settings no row
   declares, which is the one way it could be right about the count and wrong
   about the fields. */
TEST_CASE("every setting listed as undeclared is one no row declares", "[settingstable]")
{
	size_t count = 0;
	const UndeclaredSetting *listed = settingsUndeclared(count);

	REQUIRE(listed != NULL);
	REQUIRE(count > 0);

	std::map<std::string, size_t> declared;
	for (size_t i = 0; i < settingsTableCount(); ++i)
		if (settingsTable()[i].field.name != NULL)
			declared[settingsTable()[i].field.name]++;

	std::map<std::string, size_t> seen;
	for (size_t i = 0; i < count; ++i)
	{
		const UndeclaredSetting &u = listed[i];

		INFO("entry " << i);
		REQUIRE(u.where.name != NULL);
		INFO("field " << u.where.name);

		// A reason is the whole point of listing it rather than leaving it out.
		REQUIRE(u.reason != NULL);
		CHECK(u.reason[0] != '\0');

		// The same field written by the same macro a row writes, so what is
		// listed is a setting and not a name.
		const bool number = (u.where.read_number != NULL);
		const bool text = (u.where.read_text != NULL);
		CHECK(number != text);

		CHECK(declared.count(u.where.name) == 0);
		seen[u.where.name]++;
		CHECK(seen[u.where.name] == 1);
	}

	INFO("listed as undeclared: " << count);
	CHECK(count > 0);
}

/* The rows the build links against the rows the source declares. Every case here and
   beside it walks settingsTable(), so a section left out of the join takes its rows out
   of every one of them at once and each still ends in a count above nought. Removing one
   insert from the join dropped a fifth of the suite's assertions and nothing said so.

   The accessors are named here rather than counted, because a count is what a dropped
   section still satisfies. That this list is the same list the header declares is
   checked as text beside the suite. */
namespace
{

struct SectionTable
{
	const char             *name;
	const Descriptor *(*rows)(size_t &count);
};

const SectionTable kSectionTables[] =
{
	{ "audio",     &settingsTableAudio },
	{ "osd",       &settingsTableOsd },
	{ "misc",      &settingsTableMisc },
	{ "video",     &settingsTableVideo },
	{ "recording", &settingsTableRecording },
	{ "channel",   &settingsTableChannel },
	{ "network",   &settingsTableNetwork },
	{ "weather",   &settingsTableWeather },
	{ "keys",      &settingsTableKeys },
	{ "display",   &settingsTableDisplay },
	{ "player",    &settingsTablePlayer },
	{ "parental",  &settingsTableParental },
	{ "cam",       &settingsTableCam },
	{ "hdd",       &settingsTableHdd },
	{ "update",    &settingsTableUpdate }
};

std::string text(const char *s)
{
	return (s != NULL) ? std::string(s) : std::string("(none)");
}

} // namespace

TEST_CASE("every row a section table answers with is one the joined table carries", "[settingstable]")
{
	REQUIRE(settingsTableCount() > 0);

	std::map<std::string, const Descriptor *> linked;
	for (size_t i = 0; i < settingsTableCount(); ++i)
		linked[settingsTable()[i].key] = &settingsTable()[i];

	size_t total = 0;
	for (size_t s = 0; s < sizeof(kSectionTables) / sizeof(kSectionTables[0]); ++s)
	{
		size_t count = 0;
		const Descriptor *rows = kSectionTables[s].rows(count);

		INFO("section table " << kSectionTables[s].name);
		REQUIRE(rows != NULL);
		REQUIRE(count > 0);
		total += count;

		for (size_t i = 0; i < count; ++i)
		{
			INFO("row " << rows[i].key << " of the " << kSectionTables[s].name << " table");
			std::map<std::string, const Descriptor *>::const_iterator it = linked.find(rows[i].key);
			CHECK(it != linked.end());
			if (it == linked.end())
				continue;

			// The row and not merely the key, so a join that put one section's
			// rows in twice under another's name is refused as well.
			CHECK(it->second->type == rows[i].type);
			CHECK(text(it->second->section) == text(rows[i].section));
			CHECK(text(it->second->label_key) == text(rows[i].label_key));
			CHECK(text(it->second->field.name) == text(rows[i].field.name));
		}
	}

	INFO("rows the section tables answer with: " << total
	     << ", rows the joined table carries: " << settingsTableCount());
	REQUIRE(settingsTableCount() >= total);
}

/* The builders are what every table is written with, and the tables use only some
   of what they offer. Each case writes the same row twice, once through the
   builders and once by position, and holds the two to each other member by member:
   a builder that put a value in the wrong member would read as right on the page
   and be wrong in every row that used it. The builders are constexpr, so the
   arrays here being constants is part of what is held. */
namespace
{
bool builderBoxHas() { return true; }
bool builderEntryOffered() { return true; }
long builderDefault() { return 3; }
long builderLow() { return 1; }
long builderHigh() { return 9; }
bool builderChoices(std::vector<SettingChoice> &) { return true; }

constexpr long kBuilderModes[] = { 1, 2 };
constexpr Condition kBuilderInner[] =
{
	when("inner_a").is(1),
	when("inner_b").atLeast(2)
};
constexpr Condition kBuilderConditions[] =
{
	when("c_eq").is(1),
	when("c_ne").isNot(2),
	when("c_lt").below(3),
	when("c_le").atMost(4),
	when("c_gt").above(5),
	when("c_ge").atLeast(6),
	when("c_in").oneOf(kBuilderModes),
	when("c_text").textValid("placeholder"),
	anyOf(kBuilderInner)
};
const Condition kBuilderByPosition[] =
{
	{ "c_eq", CompareOp::Eq, 1, NULL, 0, NULL, NULL, 0 },
	{ "c_ne", CompareOp::Ne, 2, NULL, 0, NULL, NULL, 0 },
	{ "c_lt", CompareOp::Lt, 3, NULL, 0, NULL, NULL, 0 },
	{ "c_le", CompareOp::Le, 4, NULL, 0, NULL, NULL, 0 },
	{ "c_gt", CompareOp::Gt, 5, NULL, 0, NULL, NULL, 0 },
	{ "c_ge", CompareOp::Ge, 6, NULL, 0, NULL, NULL, 0 },
	{ "c_in", CompareOp::In, 0, kBuilderModes, 2, NULL, NULL, 0 },
	{ "c_text", CompareOp::TextValid, 0, NULL, 0, "placeholder", NULL, 0 },
	{ NULL, CompareOp::Eq, 0, NULL, 0, NULL, kBuilderInner, 2 }
};

constexpr EnumValue kBuilderEntries[] =
{
	option(0).label("options.off"),
	option(1).text("ext4").availableIf(builderEntryOffered),
	option(2).label("options.on").offeredWhen(kBuilderInner)
};
const EnumValue kBuilderEntriesByPosition[] =
{
	{ 0, "options.off", NULL, NULL, NULL, 0 },
	{ 1, NULL, "ext4", builderEntryOffered, NULL, 0 },
	{ 2, "options.on", NULL, NULL, kBuilderInner, 2 }
};

constexpr TextRule kBuilderRule = { TextKind::Pin, 4, 4, "0123456789", MustExist::No, NULL, false };

bool sameField(const FieldRef &a, const FieldRef &b)
{
	return a.read_number == b.read_number && a.write_number == b.write_number &&
	       a.fits_number == b.fits_number && a.read_text == b.read_text &&
	       a.write_text == b.write_text && a.ask == b.ask && a.tell == b.tell && a.choices == b.choices &&
	       a.name == b.name && a.origin == b.origin && a.available == b.available && a.otherwise == b.otherwise;
}

bool sameDescriptor(const Descriptor &a, const Descriptor &b)
{
	return a.key == b.key && a.type == b.type && a.section == b.section && a.label_key == b.label_key &&
	       a.hint_key == b.hint_key && a.min == b.min && a.max == b.max && a.values == b.values &&
	       a.value_count == b.value_count && a.default_int == b.default_int &&
	       a.default_string == b.default_string && a.needs_restart == b.needs_restart && a.secret == b.secret &&
	       a.conditions == b.conditions && a.condition_count == b.condition_count && sameField(a.field, b.field) &&
	       a.unit_key == b.unit_key && a.format_key == b.format_key && a.text == b.text &&
	       a.choices_from == b.choices_from && a.min_now == b.min_now && a.max_now == b.max_now &&
	       a.default_fn == b.default_fn;
}

bool sameCondition(const Condition &a, const Condition &b)
{
	return std::string(a.key == NULL ? "" : a.key) == (b.key == NULL ? "" : b.key) && (a.key == NULL) == (b.key == NULL) &&
	       a.op == b.op && a.value == b.value && a.values == b.values && a.value_count == b.value_count &&
	       (a.text == NULL) == (b.text == NULL) && (a.text == NULL || std::string(a.text) == b.text) &&
	       a.any_of == b.any_of && a.any_count == b.any_count;
}

bool sameEntry(const EnumValue &a, const EnumValue &b)
{
	return a.value == b.value && (a.label_key == NULL) == (b.label_key == NULL) &&
	       (a.label_key == NULL || std::string(a.label_key) == b.label_key) &&
	       (a.label_text == NULL) == (b.label_text == NULL) &&
	       (a.label_text == NULL || std::string(a.label_text) == b.label_text) && a.available == b.available &&
	       a.when == b.when && a.when_count == b.when_count;
}

// Whether two rows written to say the same agree in every member that holds text,
// which a pointer comparison cannot say for two literals.
bool sameRowText(const Descriptor &a, const Descriptor &b)
{
	const char *x[] = { a.key, a.section, a.label_key, a.hint_key, a.default_string, a.unit_key, a.format_key };
	const char *y[] = { b.key, b.section, b.label_key, b.hint_key, b.default_string, b.unit_key, b.format_key };
	for (size_t i = 0; i < sizeof(x) / sizeof(x[0]); ++i)
	{
		if ((x[i] == NULL) != (y[i] == NULL) || (x[i] != NULL && std::string(x[i]) != y[i]))
			return false;
	}
	return true;
}

// Pointers to literals differ between two spellings of one text, so the rows are
// held to each other with the text members blanked once they have been compared.
Descriptor withoutText(Descriptor d)
{
	d.key = d.section = d.label_key = d.hint_key = d.default_string = d.unit_key = d.format_key = NULL;
	d.field.name = NULL;
	return d;
}
} // anonymous namespace

TEST_CASE("a condition built by name is the one written by position", "[settingstable][builders]")
{
	const size_t n = sizeof(kBuilderConditions) / sizeof(kBuilderConditions[0]);
	REQUIRE(n == sizeof(kBuilderByPosition) / sizeof(kBuilderByPosition[0]));
	for (size_t i = 0; i < n; ++i)
	{
		INFO("condition " << i);
		CHECK(sameCondition(kBuilderConditions[i], kBuilderByPosition[i]));
	}
	CHECK(conditionIsGroup(kBuilderConditions[n - 1]));
}

TEST_CASE("an entry built by name is the one written by position", "[settingstable][builders]")
{
	const size_t n = sizeof(kBuilderEntries) / sizeof(kBuilderEntries[0]);
	REQUIRE(n == sizeof(kBuilderEntriesByPosition) / sizeof(kBuilderEntriesByPosition[0]));
	for (size_t i = 0; i < n; ++i)
	{
		INFO("entry " << i);
		CHECK(sameEntry(kBuilderEntries[i], kBuilderEntriesByPosition[i]));
	}
}

TEST_CASE("a row built by name is the one written by position", "[settingstable][builders]")
{
	// Every call a row has, so a call that set the wrong member shows here.
	const Descriptor built = intRow("builder_row")
		.section("builder")
		.label("builder.label")
		.hint("builder.hint")
		.range(1, 9)
		.defaultValue(2)
		.defaultFrom(builderDefault)
		.values(kBuilderEntries)
		.unit("builder.unit")
		.text(kBuilderRule)
		.choicesFrom(builderChoices)
		.minNow(builderLow)
		.maxNow(builderHigh)
		.needsRestart()
		.secret()
		.changeableWhen(kBuilderConditions)
		.availableIf(builderBoxHas)
		.field(COREAPI_NUMBER_FIELD(auto_lang));

	Descriptor by_position = {
		"builder_row", ValueType::Int, "builder", "builder.label", "builder.hint", 1, 9,
		kBuilderEntries, 3, 2, NULL, true, true, kBuilderConditions, 9,
		COREAPI_NUMBER_FIELD(auto_lang), "builder.unit", NULL, &kBuilderRule, builderChoices, builderLow,
		builderHigh, builderDefault
	};
	by_position.field.available = builderBoxHas;

	CHECK(sameRowText(built, by_position));
	CHECK(sameDescriptor(withoutText(built), withoutText(by_position)));
}

TEST_CASE("the kinds of row start from the bounds and defaults their kind has", "[settingstable][builders]")
{
	const Descriptor flag = boolRow("k").field(COREAPI_NO_FIELD);
	CHECK(flag.type == ValueType::Bool);
	CHECK(flag.min == 0);
	CHECK(flag.max == 1);
	CHECK(flag.default_string == NULL);

	const Descriptor choice = enumRow("k").field(COREAPI_NO_FIELD);
	CHECK(choice.type == ValueType::Enum);
	CHECK(choice.max == 0);

	const Descriptor words = textRow("k").defaultValue("").field(COREAPI_NO_FIELD);
	CHECK(words.type == ValueType::String);
	CHECK(words.default_string != NULL);
	CHECK(std::string(words.default_string).empty());
	CHECK(words.default_int == 0);

	// What the field macro names stays unless the row says otherwise, and the row's
	// own test wins over it.
	const Descriptor kept = boolRow("k").field(COREAPI_NUMBER_FIELD_ON(auto_lang, builderBoxHas, NULL));
	CHECK(kept.field.available == builderBoxHas);
	const Descriptor over = boolRow("k").availableIf(builderEntryOffered)
		.field(COREAPI_NUMBER_FIELD_ON(auto_lang, builderBoxHas, NULL));
	CHECK(over.field.available == builderEntryOffered);
	const Descriptor none = boolRow("k").field(COREAPI_NUMBER_FIELD(auto_lang));
	CHECK(none.field.available == NULL);
}

TEST_CASE("the single-bound, format and default calls each set their own member", "[settingstable][builders]")
{
	const Descriptor number = intRow("builder_number")
		.section("builder")
		.label("builder.label")
		.min(-4)
		.max(40)
		.format("builder.format")
		.defaultValue(7L)
		.field(COREAPI_NUMBER_FIELD(auto_lang));
	const Descriptor number_by_position = {
		"builder_number", ValueType::Int, "builder", "builder.label", NULL, -4, 40,
		NULL, 0, 7, NULL, false, false, NULL, 0,
		COREAPI_NUMBER_FIELD(auto_lang), NULL, "builder.format", NULL, NULL, NULL, NULL, NULL
	};
	CHECK(sameRowText(number, number_by_position));
	CHECK(sameDescriptor(withoutText(number), withoutText(number_by_position)));

	const Descriptor words = textRow("builder_text")
		.section("builder")
		.label("builder.label")
		.defaultValue("fallback")
		.field(COREAPI_TEXT_FIELD(language));
	const Descriptor words_by_position = {
		"builder_text", ValueType::String, "builder", "builder.label", NULL, 0, 0,
		NULL, 0, 0, "fallback", false, false, NULL, 0,
		COREAPI_TEXT_FIELD(language), NULL, NULL, NULL, NULL, NULL, NULL, NULL
	};
	CHECK(sameRowText(words, words_by_position));
	CHECK(sameDescriptor(withoutText(words), withoutText(words_by_position)));
}

/* A section every row has: a row built without the call would compile and carry
   none. A label is absent on purpose for most of what the settings file holds, no
   screen naming them, so what is held is the part that cannot be on purpose: a row
   that offers a hint or labels its choices has a name to go with it. */
TEST_CASE("every shipped row names a section, and a row with a hint names a label", "[settingstable][builders]")
{
	size_t rows = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		INFO("row " << d.key);
		CHECK(d.section != NULL);
		if (d.hint_key != NULL)
			CHECK(d.label_key != NULL);
		++rows;
	}
	CHECK(rows > 0);
}
