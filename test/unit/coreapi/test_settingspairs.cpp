/*
 * test_settingspairs.cpp - tests for settings that come in pairs
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
#include "coreapi/settings/settingstable.h"

#include "support/counts.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

/* What the program says about its own settings, read out of its source by the scripts
   beside this file and compared against what the table declares.

   The table is written by hand off the load site and off the screen, and the first five
   rows written that way carried four errors between them, every one of them a plausible
   value and none of them visible to anything that ran: a key paired with the wrong
   field, a bound read off the wrong argument and a label naming a different locale all
   compile, all load, and all read as correct on the page.

   The scripts refuse to print a short answer, so a regex that stopped matching stops the
   build rather than leaving these cases comparing nothing. The counts below are asserted
   here as well, because a check that ran over an empty map looks exactly like a pass. */

using namespace coreapi;

namespace
{

/* Keys the program stores but does not load and save in the one pass the map is read
   from, so no pair for them exists to be found. The first two are loaded through a
   conditional rather than an assignment; the third is loaded through one as well and is
   saved from a value another object holds. Named here rather than left to fail, and
   short on purpose: a list that grows is a check being talked out of its job. */
const char *const kUnpaired[] =
{
	"cpufreq",
	"standby_cpufreq",
	"osd_resolution"
};

bool isUnpaired(const std::string &key)
{
	for (size_t i = 0; i < sizeof(kUnpaired) / sizeof(kUnpaired[0]); ++i)
	{
		if (key == kUnpaired[i])
			return true;
	}
	return false;
}

/* Whether the settings file names this row's key at all, which is what every case below
   the first one really asks.

   Two answers go into it. A row whose value is not in the member it is named after is
   usually not in that file under any key either: a bit of a mask is stored under the
   mask's own key, and a value a daemon keeps is in the daemon's own file. But one of
   them is: the size the box draws at lives in another object of the program, and the
   program writes that object's copy to its own file under this row's key.

   What keeps this from being a way out of the cases below is the case at the foot of
   this file: every row it excludes has to have a key the program really does not
   read. */
bool inSettingsFile(const Descriptor &d)
{
	return valueIsInNamedMember(d.field) || isUnpaired(d.key);
}

std::vector<std::string> split(const std::string &line)
{
	std::vector<std::string> out;
	std::string cur;
	for (size_t i = 0; i < line.size(); ++i)
	{
		if (line[i] == '\t')
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += line[i];
	}
	out.push_back(cur);
	return out;
}

// The files are written by make before the suite runs. A missing one is a
// failure of this case and not something to work around, so nothing here
// invents an empty answer for it.
std::vector<std::vector<std::string> > readTable(const char *path)
{
	std::vector<std::vector<std::string> > rows;
	FILE *f = fopen(path, "r");
	if (f == NULL)
		return rows;

	std::string line;
	int c;
	while ((c = fgetc(f)) != EOF)
	{
		if (c == '\n')
		{
			if (!line.empty())
				rows.push_back(split(line));
			line.clear();
		}
		else
			line += (char) c;
	}
	if (!line.empty())
		rows.push_back(split(line));

	fclose(f);
	return rows;
}

// One bound the screens state. Several rows can name the same field, because
// one setting is offered on more than one screen where the boxes differ.
struct Offered
{
	std::string min;
	std::string max;
	std::string label;
	std::string where;
};

/* One default the program falls back to. A key loaded in two conditional arms
   has one of these for each, so a row is held to whichever arm the build it is
   written for takes rather than to only the first. kind is int, str, expr or
   none, and the last two are what no scan can turn into a value. */
struct Fallback
{
	std::string kind;
	std::string value;
};

struct Loaded
{
	std::string             field;
	std::vector<Fallback>   defaults;
};

const std::map<std::string, Loaded> &loaded()
{
	static std::map<std::string, Loaded> m;
	static bool done = false;
	if (!done)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_PAIRS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 4)
				continue;
			Loaded &l = m[rows[i][0]];
			l.field = rows[i][1];
			Fallback f;
			f.kind = rows[i][2];
			f.value = rows[i][3];
			l.defaults.push_back(f);
		}
		done = true;
	}
	return m;
}

std::string fallbacks(const std::vector<Fallback> &v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i > 0)
			s += ", ";
		s += v[i].kind + " " + v[i].value;
	}
	return s;
}

const std::set<std::string> &keysRead()
{
	static std::set<std::string> s;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_KEYS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
			s.insert(rows[i][0]);
		loaded = true;
	}
	return s;
}

const std::set<std::string> &localeNames()
{
	static std::set<std::string> s;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_LOCALES_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
			s.insert(rows[i][0]);
		loaded = true;
	}
	return s;
}

const std::map<std::string, std::vector<Offered> > &offered()
{
	static std::map<std::string, std::vector<Offered> > m;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_BOUNDS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 5)
				continue;
			Offered o;
			o.min = rows[i][1];
			o.max = rows[i][2];
			o.label = rows[i][3];
			o.where = rows[i][4];
			m[rows[i][0]].push_back(o);
		}
		loaded = true;
	}
	return m;
}

/* One label a screen states beside a setting it offers as neither a range nor a
   table of choices. The same field can be stated on more than one screen, as a
   range can. */
struct Stated
{
	std::string label;
	std::string where;
};

const std::map<std::string, std::vector<Stated> > &stated()
{
	static std::map<std::string, std::vector<Stated> > m;
	static bool loaded = false;
	if (!loaded)
	{
		std::vector<std::vector<std::string> > rows = readTable(COREAPI_LABELS_FILE);
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (rows[i].size() != 3)
				continue;
			Stated t;
			t.label = rows[i][1];
			t.where = rows[i][2];
			m[rows[i][0]].push_back(t);
		}
		loaded = true;
	}
	return m;
}

std::string states(const std::vector<Stated> &v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i > 0)
			s += ", ";
		s += v[i].where + " " + v[i].label;
	}
	return s;
}

/* The rows of the bound scan a comparison can use at all. A bound written as a
   name the scan cannot turn into a number is printed as one it could not, and
   the scan used to hold itself to how many were left. */
size_t resolvedBounds()
{
	size_t n = 0;
	for (std::map<std::string, std::vector<Offered> >::const_iterator it = offered().begin();
	     it != offered().end(); ++it)
	{
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (it->second[j].min != "?" && it->second[j].max != "?")
				++n;
		}
	}
	return n;
}

std::string sites(const std::vector<Offered> &v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i > 0)
			s += ", ";
		s += v[i].where + " " + v[i].min + ".." + v[i].max + " " + v[i].label;
	}
	return s;
}

} // namespace

TEST_CASE("the map read out of the program is the size the program has", "[settingspairs]")
{
	INFO("key to field pairs read: " << loaded().size());
	REQUIRE(loaded().size() > 0);
	recordCount("key to field pairs read out of the program", loaded().size());

	size_t comparable = 0;
	for (std::map<std::string, Loaded>::const_iterator it = loaded().begin();
	     it != loaded().end(); ++it)
	{
		for (size_t j = 0; j < it->second.defaults.size(); ++j)
		{
			if (it->second.defaults[j].kind == "int" || it->second.defaults[j].kind == "str")
			{
				++comparable;
				break;
			}
		}
	}
	INFO("keys whose default the scan can compare: " << comparable);
	recordCount("keys whose default the scan can compare", comparable);

	INFO("keys read anywhere in the tree: " << keysRead().size());
	recordCount("keys read anywhere in the tree", keysRead().size());

	size_t bounds = 0;
	for (std::map<std::string, std::vector<Offered> >::const_iterator it = offered().begin();
	     it != offered().end(); ++it)
		bounds += it->second.size();
	INFO("bounds read off the screens: " << bounds << " over " << offered().size() << " fields");
	recordCount("bounds read off the screens", bounds);
	recordCount("fields the bound scan read", offered().size());
	// The half of that scan a comparison can use, which is the count the scan
	// itself used to be held to.
	recordCount("bounds read with both ends resolved", resolvedBounds());

	INFO("locale names the program has: " << localeNames().size());
	recordCount("locale names the program has", localeNames().size());

	size_t pairings = 0;
	for (std::map<std::string, std::vector<Stated> >::const_iterator it = stated().begin();
	     it != stated().end(); ++it)
		pairings += it->second.size();
	INFO("labels stated beside a field: " << pairings << " over " << stated().size() << " fields");
	recordCount("labels stated beside a field", pairings);
	recordCount("fields with a stated label", stated().size());
}

TEST_CASE("every declared row names the field the program loads its key into", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL)
			continue;

		INFO("row " << d.key << " points at g_settings." << d.field.name);
		if (isUnpaired(d.key) || !inSettingsFile(d))
			continue;

		std::map<std::string, Loaded>::const_iterator it = loaded().find(d.key);
		CHECK(it != loaded().end());
		if (it == loaded().end())
			continue;

		++checked;
		CHECK(it->second.field == std::string(d.field.name));
	}

	INFO("rows compared against the load and save pass: " << checked);
	recordCount("rows compared against the load and save pass", checked);
}

/* The weaker question, and the only one that can be asked of a setting the
   program reads outside the one pass the map above comes from. Font sizes are
   read from the screens that use them, so a row for one has no save beside its
   load to pair with, but its key is still a key the program reads. */
TEST_CASE("every declared key is one the program reads", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (!inSettingsFile(d))
			continue;
		++checked;
		INFO("row " << d.key);
		CHECK(keysRead().count(d.key) == 1);
	}

	INFO("keys compared: " << checked);
	recordCount("keys compared against what the program reads", checked);
}

/* The default, which is the third thing on the line the pairing already reads and the
   widest check there is: the program states one for nearly every setting, against the
   few dozen whose range a screen states. Two of the three errors that last survived a
   whole run were defaults. A default the program writes as an expression cannot be
   compared and is named rather than passed over, because a row nobody can check is a
   fact about the check and not about the row. */
TEST_CASE("every declared default is the one the program falls back to", "[settingspairs]")
{
	size_t checked = 0;
	std::string uncheckable;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || !inSettingsFile(d))
			continue;

		std::map<std::string, Loaded>::const_iterator it = loaded().find(d.key);
		if (it == loaded().end())
			continue;

		const std::vector<Fallback> &v = it->second.defaults;
		bool agrees = false;
		bool comparable = false;

		for (size_t j = 0; j < v.size(); ++j)
		{
			/* An identifier is a number where the program loads it and text
			   here, so the two are compared as the number both of them mean.
			   Read as a spelling against a fallback that was never written as
			   one, it would be a row nothing could check. */
			if (d.field.origin == FieldOrigin::ChannelIdField)
			{
				if (v[j].kind != "int")
					continue;
				comparable = true;
				unsigned long long declared = 0;
				if (d.default_string != NULL &&
				    readChannelIdText(d.default_string, declared) &&
				    (unsigned long long) strtoll(v[j].value.c_str(), NULL, 10) == declared)
					agrees = true;
			}
			// A String carries its default as text and every other kind as a
			// number, so only the matching sort of fallback answers for it.
			else if (d.type == ValueType::String)
			{
				if (v[j].kind != "str")
					continue;
				comparable = true;
				if (d.default_string != NULL && v[j].value == std::string(d.default_string))
					agrees = true;
			}
			else
			{
				if (v[j].kind != "int")
					continue;
				comparable = true;
				if (strtol(v[j].value.c_str(), NULL, 10) == d.default_int)
					agrees = true;
			}
		}

		if (!comparable)
		{
			uncheckable += (uncheckable.empty() ? "" : ", ");
			uncheckable += d.key + std::string(" [") + fallbacks(v) + "]";
			continue;
		}

		std::string declared;
		if (d.field.origin == FieldOrigin::ChannelIdField)
			declared = std::string("id ") + (d.default_string != NULL ? d.default_string : "");
		else if (d.type == ValueType::String)
			declared = std::string("str ") + (d.default_string != NULL ? d.default_string : "");
		else
		{
			char n[32];
			snprintf(n, sizeof(n), "int %ld", d.default_int);
			declared = n;
		}

		++checked;
		INFO("row " << d.key << " declares " << declared
		     << ", the program falls back to " << fallbacks(v));
		CHECK(agrees);
	}

	INFO("defaults compared: " << checked
	     << "; rows whose default the program writes as an expression: " << uncheckable);
	recordCount("defaults compared against the program", checked);
}

/* The bounds, which are the ones the pairing above cannot see. Both of the two
   wrong rows the review found named the right field for the right key and got
   the range wrong, one of them by reading the minimum as a maximum. */
TEST_CASE("every declared bound is the one the screen enforces", "[settingspairs]")
{
	size_t checked = 0;
	size_t skippedKind = 0;
	size_t skippedUnresolved = 0;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL)
			continue;

		std::map<std::string, std::vector<Offered> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		// Only a range means a range. A choice among named values carries no
		// bound of its own and would fail against one.
		if (d.type != ValueType::Int)
		{
			++skippedKind;
			continue;
		}

		bool agrees = false;
		bool comparable = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			const Offered &o = it->second[j];
			if (o.min == "?" || o.max == "?")
				continue;
			comparable = true;
			if (strtol(o.min.c_str(), NULL, 10) == d.min &&
			    strtol(o.max.c_str(), NULL, 10) == d.max)
				agrees = true;
		}

		if (!comparable)
		{
			++skippedUnresolved;
			continue;
		}

		++checked;
		INFO("row " << d.key << " declares " << d.min << ".." << d.max
		     << ", the screens offer " << sites(it->second));
		CHECK(agrees);
	}

	INFO("bounds compared: " << checked << ", not a range: " << skippedKind
	     << ", bound not a number in the source: " << skippedUnresolved);
	recordCount("bounds compared against a screen", checked);
}

/* The weaker question about a label, and the one that can be asked of every row
   rather than only of the ones a number chooser offers. A label naming nothing
   shows as the key itself on the screen, which reads as a missing translation
   and not as a wrong row. */
TEST_CASE("every declared label names a locale the program has", "[settingspairs]")
{
	size_t checked = 0;
	size_t nameless = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];

		if (d.label_key == NULL)
		{
			++nameless;
		}
		else
		{
			++checked;
			INFO("row " << d.key << " declares label " << d.label_key);
			CHECK(localeNames().count(d.label_key) == 1);
		}

		if (d.hint_key != NULL)
		{
			INFO("row " << d.key << " declares hint " << d.hint_key);
			CHECK(localeNames().count(d.hint_key) == 1);
		}
	}

	INFO("labels compared: " << checked << ", rows the program has no name for: " << nameless);
	recordCount("labels compared against the locale names", checked);
}

/* The other half of allowing a row to say it has no name: one that says so
   while a screen does name the field is a name thrown away, and a frontend
   would then show the key where the box shows a word. This reads the ranges the
   screens offer; the case beside it in the choices file reads the choosers. */
TEST_CASE("a row with no label is one no number chooser names", "[settingspairs]")
{
	size_t nameless = 0;
	std::string named;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.label_key != NULL || d.field.name == NULL)
			continue;

		++nameless;
		std::map<std::string, std::vector<Offered> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		named += (named.empty() ? "" : ", ");
		named += std::string(d.key) + " " + sites(it->second);
	}

	INFO("rows with no label that a number chooser does name: " << named);
	CHECK(named.empty());

	INFO("rows the program has no name for: " << nameless);
	REQUIRE(nameless > 0);
}

/* The label, which the pairing cannot see either. One of the five rows named a
   locale that exists and belongs to a different item, which reads as right on
   the page and is wrong on the screen. */
TEST_CASE("every declared label is the one the screen offers", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || d.label_key == NULL)
			continue;

		std::map<std::string, std::vector<Offered> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		bool agrees = false;
		bool comparable = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (it->second[j].label == "?")
				continue;
			comparable = true;
			if (it->second[j].label == std::string(d.label_key))
				agrees = true;
		}

		if (!comparable)
			continue;

		++checked;
		INFO("row " << d.key << " declares label " << d.label_key
		     << ", the screens offer " << sites(it->second));
		CHECK(agrees);
	}

	INFO("labels compared: " << checked);
	recordCount("labels compared against a number chooser", checked);
}

/* The rest of the rows, which neither the case above nor the one in the choices file
   reaches: a setting offered through a forwarder, a string chooser or one of the text
   inputs has a label nothing compares, and a wrong one there is a locale that exists,
   belonging to the item beside it on the screen. That is the error the first five rows
   carried.

   The label is read out of the argument each widget carries the setting in and not out
   of the whole call, because a forwarder takes whether it is active in the argument
   before the one it shows. Read whole, seven rows here pair a label with a setting the
   screen only asks about. */
TEST_CASE("every declared label is the one the screen states beside its field", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || d.label_key == NULL)
			continue;

		std::map<std::string, std::vector<Stated> >::const_iterator it =
			stated().find(d.field.name);
		if (it == stated().end())
			continue;

		bool agrees = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (it->second[j].label == std::string(d.label_key))
				agrees = true;
		}

		++checked;
		INFO("row " << d.key << " declares label " << d.label_key
		     << ", the screens state " << states(it->second));
		CHECK(agrees);
	}

	INFO("labels compared: " << checked);
	recordCount("labels compared against a stated field", checked);
}

/* The names a choice offers, which no check above reads: a row's own label is
   compared against the screen, and the words beside each of its values are not.
   One naming nothing shows on screen as the name itself. */
TEST_CASE("every value a choice offers names a locale the program has", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		for (size_t v = 0; v < d.value_count; ++v)
		{
			++checked;
			INFO("row " << d.key << " offers " << d.values[v].value
			     << " as " << (d.values[v].label_key ? d.values[v].label_key : "(none)"));
			CHECK(d.values[v].label_key != NULL);
			if (d.values[v].label_key != NULL)
				CHECK(localeNames().count(d.values[v].label_key) == 1);
		}
	}

	INFO("choices compared: " << checked);
	recordCount("choice value names compared", checked);
}

/* The rows the three cases above step over, held to what they do say rather than left
   out of every check at once.

   Each of them declares that its value is not in the settings file, and the one thing
   that can be asked of that declaration here is whether it is true: a row whose key the
   program does read is one the cases above should have held to the file. The rest of
   what each kind promises is a question about running code and is asked where code
   runs.

   The count is asserted for the reason every count here is: a table that declared none
   of these would satisfy every line above by looking at nothing. */
TEST_CASE("a row whose value is not in the settings file is not in it", "[settingspairs]")
{
	size_t checked = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (inSettingsFile(d))
			continue;

		++checked;
		INFO("row " << d.key << " says its value is not in the settings file");
		CHECK(keysRead().count(d.key) == 0);
		CHECK(loaded().find(d.key) == loaded().end());

		/* And it still names a member, because that is what the cases reading
		   the screens find its bounds, its label and its choices under. */
		INFO("row " << d.key << " is found under g_settings." << (d.field.name ? d.field.name : "(none)"));
		CHECK(d.field.name != NULL);
	}

	INFO("rows whose value is not in the settings file: " << checked);
	recordCount("rows whose value is not in the settings file", checked);
	REQUIRE(checked > 0);
}
