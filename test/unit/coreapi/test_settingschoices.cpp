/*
 * test_settingschoices.cpp - tests for the choices a setting offers
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

/* What a screen offers a setting as, read out of its source by the script beside this
   and compared against what the table declares.

   The pairing, the bounds, the defaults and the labels are each held to the program
   elsewhere. The values a choice offers are held to nothing: a row can offer a value the
   screen does not, leave out one it does, or put the wrong words beside one, and every
   other case here still passes.

   The script refuses to print a short answer, so a scan that stopped matching stops the
   build rather than leaving these cases comparing nothing. */

#include "support/counts.h"

using namespace coreapi;

namespace
{

/* The words the program names an on and an off by. A setting the screen offers
   under these two and no others is one whose stored value a Bool carries, and
   which of the two is one is then a fact the declaration has to agree with:
   nothing in a Bool row says so, so a screen that reads nought as on turns a
   frontend's true into the opposite. */
const char *const kOn = "options.on";
const char *const kOff = "options.off";

std::vector<std::string> split(const std::string &line, char sep)
{
	std::vector<std::string> out;
	std::string cur;
	for (size_t i = 0; i < line.size(); ++i)
	{
		if (line[i] == sep)
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

// One value a screen offers. gated says a preprocessor arm inside the screen's
// table carries it, which is a choice only some builds are given and one a row
// may leave out; resolved says the scan could turn both halves into what they
// stand for.
struct Choice
{
	long        value;
	std::string label;
	bool        gated;
	bool        resolved;
};

// One call site. A setting offered on two screens has one of these for each,
// because which of them a build shows depends on the box.
struct Site
{
	std::string         label;
	std::string         where;
	std::string         table;
	std::vector<Choice> choices;
	// Whether every entry of the table was read. A site that was not is named
	// rather than passed over: a row nobody can check is a fact about the check.
	bool                readable;
};

// The file is written by make before the suite runs. A missing one is a failure
// of these cases and not something to work around.
const std::map<std::string, std::vector<Site> > &offered()
{
	static std::map<std::string, std::vector<Site> > m;
	static bool done = false;
	if (done)
		return m;
	done = true;

	FILE *f = fopen(COREAPI_CHOICES_FILE, "r");
	if (f == NULL)
		return m;

	std::string line;
	int c;
	for (;;)
	{
		c = fgetc(f);
		if (c != EOF && c != '\n')
		{
			line += (char) c;
			continue;
		}
		if (!line.empty())
		{
			std::vector<std::string> col = split(line, '\t');
			if (col.size() == 5)
			{
				Site s;
				s.label = col[1];
				s.where = col[2];
				s.table = col[3];
				s.readable = !col[4].empty();

				std::vector<std::string> entries = split(col[4], ',');
				for (size_t i = 0; s.readable && i < entries.size(); ++i)
				{
					std::string e = entries[i];
					Choice ch;
					ch.gated = !e.empty() && e[e.size() - 1] == '!';
					if (ch.gated)
						e.erase(e.size() - 1);
					size_t eq = e.find('=');
					ch.resolved = eq != std::string::npos &&
					              e.substr(0, eq) != "?" && e.substr(eq + 1) != "?";
					ch.value = ch.resolved ? strtol(e.substr(0, eq).c_str(), NULL, 10) : 0;
					ch.label = eq == std::string::npos ? "?" : e.substr(eq + 1);
					s.choices.push_back(ch);
				}
				m[col[0]].push_back(s);
			}
		}
		line.clear();
		if (c == EOF)
			break;
	}

	fclose(f);
	return m;
}

bool fullyRead(const Site &s)
{
	if (!s.readable)
		return false;
	for (size_t i = 0; i < s.choices.size(); ++i)
	{
		if (!s.choices[i].resolved)
			return false;
	}
	return !s.choices.empty();
}

std::string describe(const Site &s)
{
	std::string t = s.where + " " + s.table + " ";
	if (!s.readable)
		return t + "(not read)";
	for (size_t i = 0; i < s.choices.size(); ++i)
	{
		char n[32];
		snprintf(n, sizeof(n), "%ld", s.choices[i].value);
		t += (i > 0 ? "," : "");
		t += (s.choices[i].resolved ? n : "?");
		t += "=" + s.choices[i].label + (s.choices[i].gated ? "!" : "");
	}
	return t;
}

std::string describe(const std::vector<Site> &v)
{
	std::string t;
	for (size_t i = 0; i < v.size(); ++i)
		t += (i > 0 ? "; " : "") + describe(v[i]);
	return t;
}

std::string declared(const Descriptor &d)
{
	std::string t;
	for (size_t i = 0; i < d.value_count; ++i)
	{
		char n[32];
		snprintf(n, sizeof(n), "%d", d.values[i].value);
		t += (i > 0 ? "," : "");
		t += n;
		t += std::string("=") + (d.values[i].label_key ? d.values[i].label_key : "(none)");
	}
	return t;
}

// Whether one site offers exactly what the row declares. Both directions: a
// value the row offers has to be one the screen offers under the same words,
// and a value the screen offers has to be one the row offers unless an arm
// gates it.
bool agrees(const Descriptor &d, const Site &s)
{
	for (size_t i = 0; i < d.value_count; ++i)
	{
		bool found = false;
		for (size_t j = 0; !found && j < s.choices.size(); ++j)
		{
			found = s.choices[j].value == d.values[i].value &&
			        d.values[i].label_key != NULL &&
			        s.choices[j].label == std::string(d.values[i].label_key);
		}
		if (!found)
			return false;
	}

	for (size_t j = 0; j < s.choices.size(); ++j)
	{
		if (s.choices[j].gated)
			continue;
		bool found = false;
		for (size_t i = 0; !found && i < d.value_count; ++i)
			found = d.values[i].value == s.choices[j].value;
		if (!found)
			return false;
	}

	return true;
}

} // namespace

TEST_CASE("the choices read out of the screens are as many as the screens offer", "[settingschoices]")
{
	size_t sites = 0;
	size_t resolved = 0;
	for (std::map<std::string, std::vector<Site> >::const_iterator it = offered().begin();
	     it != offered().end(); ++it)
	{
		sites += it->second.size();
		for (size_t i = 0; i < it->second.size(); ++i)
		{
			if (fullyRead(it->second[i]))
				++resolved;
		}
	}

	INFO("call sites read: " << sites << " over " << offered().size() << " fields");
	REQUIRE(sites > 0);
	recordCount("chooser call sites read", sites);
	recordCount("fields the chooser scan read", offered().size());
	INFO("call sites whose every choice the scan could read: " << resolved);
	recordCount("chooser call sites read whole", resolved);
}

/* The values, which no other case reads. A row offering a value the screen does
   not offers a frontend something the box will not take; one leaving out a
   value the screen offers hides a setting a frontend cannot reach; and one
   pairing a value with another value's words is wrong on the screen and right
   on the page. */
TEST_CASE("every value a declared choice offers is one the screen offers", "[settingschoices]")
{
	size_t checked = 0;
	size_t rows = 0;
	size_t asked = 0;
	std::string unread;
	std::string unoffered;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.type != ValueType::Enum || d.field.name == NULL)
			continue;

		/* A row whose set is the box's own carries no list to compare, and the screen's
		   table is not the comparison either: it is one of several the screen picks
		   between on a revision read at run time, or a table with the modes this model
		   cannot draw marked out. What holds these rows is check-choices.sh and the
		   cases over the layer. Counted, so a row quietly gaining a set of its own is a
		   change in a figure rather than nothing at all. */
		if (d.field.choices != NULL)
		{
			++asked;
			INFO("row " << d.key << " asks for its values under " << d.field.choices);
			CHECK(d.value_count == 0);
			continue;
		}

		++rows;

		std::map<std::string, std::vector<Site> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
		{
			// A setting no chooser governs, which is a row this cannot answer
			// for rather than a row that is wrong. Named so that what the case
			// covers is countable against what is declared.
			unoffered += (unoffered.empty() ? "" : ", ");
			unoffered += d.key;
			continue;
		}

		bool comparable = false;
		bool holds = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (!fullyRead(it->second[j]))
				continue;
			comparable = true;
			if (agrees(d, it->second[j]))
				holds = true;
		}

		if (!comparable)
		{
			unread += (unread.empty() ? "" : ", ");
			unread += d.key;
			continue;
		}

		++checked;
		INFO("row " << d.key << " offers " << declared(d)
		     << ", the screens offer " << describe(it->second));
		CHECK(holds);
	}

	/* A row the scan stopped reading is a row that stopped being checked, and
	   with a handful declared that is a green run either way. So it fails here
	   rather than being counted: either the scan is made to read the table, or
	   the row is named as one it cannot and the naming is what says so. */
	INFO("rows the scan could not read the screen table of: " << unread);
	CHECK(unread.empty());

	INFO("choices compared: " << checked << " of " << rows << " declared; no chooser governs: "
	     << (unoffered.empty() ? "none" : unoffered));
	recordCount("choices compared against a screen", checked);

	INFO("rows whose set of values the box decides: " << asked);
	recordCount("rows whose set of values the box decides", asked);
	REQUIRE(asked > 0);
}

/* A Bool carries no values of its own, so the only thing to hold it to is that
   the screen offers two and that they are the two the wire means. */
TEST_CASE("every declared flag is offered by the screen as its own two values", "[settingschoices]")
{
	size_t checked = 0;
	size_t rows = 0;
	std::string unread;
	std::string unoffered;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.type != ValueType::Bool || d.field.name == NULL)
			continue;
		++rows;

		std::map<std::string, std::vector<Site> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
		{
			unoffered += (unoffered.empty() ? "" : ", ");
			unoffered += d.key;
			continue;
		}

		bool comparable = false;
		bool holds = false;
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			const Site &s = it->second[j];
			if (!fullyRead(s))
				continue;
			comparable = true;

			// A screen offering a third choice is a setting no flag can carry,
			// so the count is part of what is asked rather than what decides
			// whether to ask.
			if (s.choices.size() != 2)
				continue;
			bool two = (s.choices[0].value == 0 && s.choices[1].value == 1) ||
			           (s.choices[0].value == 1 && s.choices[1].value == 0);
			// The wording is read only where the program uses its own two, so a
			// screen offering a yes and a no is held to the values alone.
			bool named = (s.choices[0].label == kOn || s.choices[0].label == kOff) &&
			             (s.choices[1].label == kOn || s.choices[1].label == kOff) &&
			             s.choices[0].label != s.choices[1].label;
			bool oneIsOn = true;
			for (size_t k = 0; named && k < 2; ++k)
			{
				if (s.choices[k].label == kOn && s.choices[k].value != 1)
					oneIsOn = false;
			}
			if (two && oneIsOn)
				holds = true;
		}

		if (!comparable)
		{
			unread += (unread.empty() ? "" : ", ");
			unread += d.key;
			continue;
		}

		++checked;
		INFO("row " << d.key << " is a flag, the screens offer " << describe(it->second));
		CHECK(holds);
	}

	INFO("rows the scan could not read the screen table of: " << unread);
	CHECK(unread.empty());

	INFO("flags compared: " << checked << " of " << rows << " declared; no chooser governs: "
	     << (unoffered.empty() ? "none" : unoffered));
	recordCount("flags compared against a screen", checked);
}

namespace
{

/* The labels the scan beside this one reads: the calls that offer a range. One setting
   is offered as a range on the boxes whose panel takes a count and as an on and an off
   on the boxes whose panel does not, and the screen picks between the two items by the
   presence of a file rather than by a setting. One field, two items, two labels, and
   each scan sees one of them, so a row declaring either is right and is refused by
   whichever scan did not see it.

   Only the label is taken; the bounds are the other scan's business. */
const std::map<std::string, std::set<std::string> > &ranged()
{
	static std::map<std::string, std::set<std::string> > m;
	static bool done = false;
	if (done)
		return m;
	done = true;

	FILE *f = fopen(COREAPI_BOUNDS_FILE, "r");
	if (f == NULL)
		return m;

	std::string line;
	for (;;)
	{
		int c = fgetc(f);
		if (c != EOF && c != '\n')
		{
			line += (char) c;
			continue;
		}
		if (!line.empty())
		{
			std::vector<std::string> col = split(line, '\t');
			if (col.size() == 5 && col[3] != "?")
				m[col[0]].insert(col[3]);
			line.clear();
		}
		if (c == EOF)
			break;
	}

	fclose(f);
	return m;
}

bool statedAsARange(const std::string &field, const std::string &label)
{
	std::map<std::string, std::set<std::string> >::const_iterator it = ranged().find(field);
	return it != ranged().end() && it->second.count(label) == 1;
}

} // namespace

/* The label a chooser states, which the case that reads the screens elsewhere
   cannot see: that one reads the calls that offer a range, and a setting
   offered as a choice is not one of them. */
TEST_CASE("every declared choice carries the label its screen states", "[settingschoices]")
{
	size_t checked = 0;
	std::string unread;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL || d.label_key == NULL)
			continue;

		std::map<std::string, std::vector<Site> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		bool comparable = false;
		bool holds = statedAsARange(d.field.name, d.label_key);
		for (size_t j = 0; j < it->second.size(); ++j)
		{
			if (it->second[j].label == "?")
				continue;
			comparable = true;
			if (it->second[j].label == std::string(d.label_key))
				holds = true;
		}

		if (!comparable)
		{
			unread += (unread.empty() ? "" : ", ");
			unread += d.key;
			continue;
		}

		++checked;
		INFO("row " << d.key << " declares label " << d.label_key
		     << ", the screens state " << describe(it->second));
		CHECK(holds);
	}

	INFO("rows whose screen states its label as something other than a name: " << unread);
	CHECK(unread.empty());

	INFO("labels compared: " << checked);
	recordCount("labels compared against a chooser", checked);
}

/* The companion of the same rule in the pairs file, over the choosers rather
   than the ranges: a row that says the program has no name for it, while a
   chooser states one, throws that name away. */
TEST_CASE("a row with no label is one no chooser names", "[settingschoices]")
{
	size_t nameless = 0;
	std::string named;

	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.label_key != NULL || d.field.name == NULL)
			continue;

		++nameless;
		std::map<std::string, std::vector<Site> >::const_iterator it =
			offered().find(d.field.name);
		if (it == offered().end())
			continue;

		named += (named.empty() ? "" : ", ");
		named += std::string(d.key) + " " + describe(it->second);
	}

	INFO("rows with no label that a chooser does name: " << named);
	CHECK(named.empty());

	INFO("rows the program has no name for: " << nameless);
	REQUIRE(nameless > 0);
}

/* Reported and not required. A setting a screen offers as a choice and a row
   declares as a number lets a frontend write a value between the offered ones,
   which the box may or may not take; there is no rule here that says the row is
   wrong, and there is nothing else that would say it at all. */
TEST_CASE("a number offered as a choice is named", "[settingschoices]")
{
	std::string named;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (d.field.name == NULL)
			continue;
		if (d.type != ValueType::Int && d.type != ValueType::String)
			continue;
		if (offered().find(d.field.name) == offered().end())
			continue;
		named += (named.empty() ? "" : ", ");
		named += d.key;
	}

	INFO("rows a screen offers as a choice and the table declares as a number or text: "
	     << (named.empty() ? "none" : named));
	CHECK(settingsTableCount() > 0);
}
