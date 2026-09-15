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
	{ "fixture_flag", CompareOp::Ne, 0, NULL, 0 }
};

const Condition kOnNothing[] =
{
	{ "fixture_no_such_key", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kSound[] =
{
	{
		"fixture_flag", ValueType::Bool, "fixture", "fixture.flag", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD
	},
	{
		"fixture_gated", ValueType::Int, "fixture", "fixture.gated", NULL,
		0, 9, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kOnRow), COREAPI_NO_FIELD
	}
};

const Descriptor kDangling[] =
{
	{
		"fixture_flag", ValueType::Bool, "fixture", "fixture.flag", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD
	},
	{
		"fixture_gated", ValueType::Int, "fixture", "fixture.gated", NULL,
		0, 9, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kOnNothing), COREAPI_NO_FIELD
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
