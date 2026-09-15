/*
 * test_settingsappliers.cpp - tests for applying settings to the GUI
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
#include <map>
#include <set>
#include <string>
#include <vector>

/* What the seam between a written setting and the box acts on, and how much of the
   table that is.

   The lists in src/gui/settings_appliers.cpp were transcribed from the notifiers by
   hand. Nothing here can link that object, so the lists are read as text by
   extract-applied.sh, which follows the registration to the applier, the applier to its
   notifier and the notifier to the options it branches on.

   The number that matters is rows and not sections. A section with an applier is a
   section where some settings are acted on, which reads as coverage and is not: eight of
   the sixteen carry an applier and the rows those eight declare outnumber the rows
   anything acts on by an order of magnitude. */

using namespace coreapi;

namespace
{

std::vector<std::string> splitTabs(const std::string &line)
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

// Written by make before the suite runs. A missing file is a failure of these
// cases and not something to invent an empty answer for.
const std::vector<std::vector<std::string> > &records()
{
	static std::vector<std::vector<std::string> > rows;
	static bool read = false;
	if (read)
		return rows;
	read = true;

	FILE *f = fopen(COREAPI_APPLIED_FILE, "r");
	if (f == NULL)
		return rows;

	std::string line;
	int c;
	while ((c = fgetc(f)) != EOF)
	{
		if (c == '\n')
		{
			if (!line.empty())
				rows.push_back(splitTabs(line));
			line.clear();
		}
		else
			line += (char) c;
	}
	if (!line.empty())
		rows.push_back(splitTabs(line));
	fclose(f);
	return rows;
}

// registered	section
const std::set<std::string> &registered()
{
	static std::set<std::string> out;
	static bool read = false;
	if (!read)
	{
		read = true;
		for (size_t i = 0; i < records().size(); ++i)
		{
			if (records()[i].size() == 2 && records()[i][0] == "registered")
				out.insert(records()[i][1]);
		}
	}
	return out;
}

// listed	section	key	locale	label
struct Listed
{
	std::string section;
	std::string key;
	std::string locale;
	std::string label;
};

const std::vector<Listed> &listed()
{
	static std::vector<Listed> out;
	static bool read = false;
	if (!read)
	{
		read = true;
		for (size_t i = 0; i < records().size(); ++i)
		{
			const std::vector<std::string> &r = records()[i];
			if (r.size() != 5 || r[0] != "listed")
				continue;
			Listed l;
			l.section = r[1];
			l.key = r[2];
			l.locale = r[3];
			l.label = r[4];
			out.push_back(l);
		}
	}
	return out;
}

// acts	section	locale	label	where
struct Acts
{
	std::string section;
	std::string label;
	std::string where;
};

const std::vector<Acts> &actsOn()
{
	static std::vector<Acts> out;
	static bool read = false;
	if (!read)
	{
		read = true;
		for (size_t i = 0; i < records().size(); ++i)
		{
			const std::vector<std::string> &r = records()[i];
			if (r.size() != 5 || r[0] != "acts")
				continue;
			Acts a;
			a.section = r[1];
			a.label = r[3];
			a.where = r[4];
			out.push_back(a);
		}
	}
	return out;
}

// Whether the section of a row has one at all, which is what makes the row part
// of the denominator the honest figure is over.
bool hasApplier(const char *section)
{
	return section != NULL && registered().count(section) == 1;
}

std::string sectionOf(const Descriptor &d)
{
	return d.section != NULL ? std::string(d.section) : std::string();
}

} // namespace

/* The scan behind the three cases below. A file that stopped matching leaves
   every one of them comparing nothing and passing, which is the shape of
   failure this whole guard exists for, so what it read is counted first. */
TEST_CASE("the seam read out of the source is the size the source has", "[settingsappliers]")
{
	INFO("records read: " << records().size());
	REQUIRE(records().size() > 0);

	recordCount("applier sections registered", registered().size());
	recordCount("keys an applier lists", listed().size());
	recordCount("options an applier or its notifier branches on", actsOn().size());
}

/* A key in a list that no row of that section declares is a key apply() answers
   true for and nothing acts on, which is the defect the key routing was written
   to close, one level up. The locale beside it has to be the row's own label
   too: the key is what routes and the locale is what the notifier is asked
   with, and a pair that names two different rows applies the wrong one. */
TEST_CASE("every key an applier lists is a row its own section declares", "[settingsappliers]")
{
	std::map<std::string, const Descriptor *> byKey;
	for (size_t i = 0; i < settingsTableCount(); ++i)
		byKey[settingsTable()[i].key] = &settingsTable()[i];

	size_t checked = 0;
	for (size_t i = 0; i < listed().size(); ++i)
	{
		const Listed &l = listed()[i];
		INFO("the " << l.section << " applier lists " << l.key << " under " << l.locale);

		std::map<std::string, const Descriptor *>::const_iterator it = byKey.find(l.key);
		// A key behind a build condition this build does not take is read by
		// the scan and linked by nothing, so it is passed over rather than
		// failed: the list answers for every box and the table for one.
		if (it == byKey.end())
			continue;

		++checked;
		CHECK(sectionOf(*it->second) == l.section);
		CHECK(it->second->label_key != NULL);
		if (it->second->label_key != NULL)
			CHECK(std::string(it->second->label_key) == l.label);
	}

	recordCount("listed keys compared against the table", checked);
}

/* The other direction, and the one a hand written list loses first: a notifier
   that gains a branch for a setting the section declares leaves the list short,
   and the row is written, saved and never applied with nothing to say so. */
TEST_CASE("every option a notifier acts on is one its applier lists", "[settingsappliers]")
{
	std::set<std::string> listedKeys;
	for (size_t i = 0; i < listed().size(); ++i)
		listedKeys.insert(listed()[i].section + "\t" + listed()[i].key);

	size_t checked = 0;
	size_t elsewhere = 0;
	for (size_t i = 0; i < actsOn().size(); ++i)
	{
		const Acts &a = actsOn()[i];
		for (size_t j = 0; j < settingsTableCount(); ++j)
		{
			const Descriptor &d = settingsTable()[j];
			if (d.label_key == NULL || sectionOf(d) != a.section || std::string(d.label_key) != a.label)
				continue;

			/* A row whose value is not in the member it is named after must not be
			   listed, and the direction is the opposite of the rule above: the
			   notifier's branch reads that member, the write went where the value
			   really lives and left the member alone, so running the notifier would
			   apply whatever the screen last left in it. Refused rather than passed
			   over, so adding such a key to a list is a failure here. */
			if (!valueIsInNamedMember(d.field))
			{
				++elsewhere;
				INFO("row " << d.key << " carries " << a.label
				     << ", which " << a.where << " acts on through a member the row does not write");
				CHECK(listedKeys.count(a.section + "\t" + std::string(d.key)) == 0);
				continue;
			}

			++checked;
			INFO("row " << d.key << " carries " << a.label
			     << ", which " << a.where << " acts on");
			CHECK(listedKeys.count(a.section + "\t" + std::string(d.key)) == 1);
		}
	}

	recordCount("acted options matched to a declared row", checked);
	// Counted, or a table that stopped declaring any of them would satisfy the
	// branch above by never taking it.
	recordCount("acted options whose row writes another member", elsewhere);
	REQUIRE(elsewhere > 0);
}

/* The figure the build states about this seam. Sections are the wrong unit:
   the eight that carry an applier declare the great majority of the table
   between them and what any applier acts on is a small part of that. Counted
   here rather than printed by a scan, so that a fall in either of them is a
   failure and not a line scrolling past. */
TEST_CASE("what an applier acts on is counted in rows and not in sections", "[settingsappliers]")
{
	std::set<std::string> listedKeys;
	for (size_t i = 0; i < listed().size(); ++i)
		listedKeys.insert(listed()[i].section + "\t" + listed()[i].key);

	size_t sectioned = 0;
	size_t acted = 0;
	for (size_t i = 0; i < settingsTableCount(); ++i)
	{
		const Descriptor &d = settingsTable()[i];
		if (!hasApplier(d.section))
			continue;
		++sectioned;
		if (listedKeys.count(sectionOf(d) + "\t" + std::string(d.key)) == 1)
			++acted;
	}

	INFO("rows in a section with an applier: " << sectioned);
	INFO("rows an applier acts on: " << acted);
	REQUIRE(acted < sectioned);

	recordCount("rows this build links", settingsTableCount());
	recordCount("rows in a section with an applier", sectioned);
	recordCount("rows an applier acts on", acted);
}
