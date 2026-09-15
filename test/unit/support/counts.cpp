/*
 * counts.cpp - counting what the tests touched, for the identity check
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

#include "counts.h"

#include <cstdio>
#include <map>
#include <string>

namespace
{

// One entry per name. A map rather than a list, so that the file this writes is
// in one order whatever order the cases ran in.
std::map<std::string, size_t> &measured()
{
	static std::map<std::string, size_t> m;
	return m;
}

// The same name recorded twice with two different values, which says two cases
// count different things under one name. Kept rather than resolved, because
// either value would be a guess.
std::map<std::string, size_t> &clashes()
{
	static std::map<std::string, size_t> m;
	return m;
}

bool readFile(const char *path, std::map<std::string, size_t> &out, std::string &why)
{
	FILE *f = std::fopen(path, "r");
	if (f == NULL)
	{
		why = "cannot be read";
		return false;
	}

	std::string line;
	int c;
	bool ok = true;
	while (ok)
	{
		c = std::fgetc(f);
		if (c != EOF && c != '\n')
		{
			line += (char) c;
			continue;
		}
		if (!line.empty() && line[0] != '#')
		{
			const size_t tab = line.find('\t');
			if (tab == std::string::npos || tab + 1 >= line.size())
			{
				why = "carries a line that is not a name and a number: " + line;
				ok = false;
			}
			else
			{
				const std::string name = line.substr(0, tab);
				const std::string value = line.substr(tab + 1);
				size_t n = 0;
				for (size_t i = 0; ok && i < value.size(); ++i)
				{
					if (value[i] < '0' || value[i] > '9')
					{
						why = "carries a value that is not a number: " + line;
						ok = false;
					}
					else
						n = n * 10 + (size_t)(value[i] - '0');
				}
				if (ok)
					out[name] = n;
			}
		}
		line.clear();
		if (c == EOF)
			break;
	}

	std::fclose(f);
	return ok;
}

bool writeFile(const char *path, const std::map<std::string, size_t> &in)
{
	FILE *f = std::fopen(path, "w");
	if (f == NULL)
		return false;
	std::fprintf(f, "# What this run compared. Regenerated on every run.\n");
	for (std::map<std::string, size_t>::const_iterator it = in.begin(); it != in.end(); ++it)
		std::fprintf(f, "%s\t%lu\n", it->first.c_str(), (unsigned long) it->second);
	std::fclose(f);
	return true;
}

} // namespace

void recordCount(const char *name, size_t value)
{
	if (name == NULL)
		return;
	std::map<std::string, size_t> &m = measured();
	std::map<std::string, size_t>::iterator it = m.find(name);
	if (it != m.end() && it->second != value)
	{
		clashes()[name] = value;
		return;
	}
	m[name] = value;
}

bool coverageCountsAgree(const char *expected_path, const char *actual_path, bool whole)
{
	writeFile(actual_path, measured());

	bool agree = true;
	for (std::map<std::string, size_t>::const_iterator it = clashes().begin();
	     whole && it != clashes().end(); ++it)
	{
		std::fprintf(stderr, "coverage count %s was recorded twice with two values, %lu and %lu\n",
			     it->first.c_str(), (unsigned long) measured()[it->first],
			     (unsigned long) it->second);
		agree = false;
	}

	std::map<std::string, size_t> expected;
	std::string why;
	if (!readFile(expected_path, expected, why))
	{
		std::fprintf(stderr, "coverage counts: %s %s\n", expected_path, why.c_str());
		return false;
	}

	for (std::map<std::string, size_t>::const_iterator it = measured().begin();
	     it != measured().end(); ++it)
	{
		std::map<std::string, size_t>::const_iterator e = expected.find(it->first);
		if (e == expected.end())
		{
			if (whole)
			{
				std::fprintf(stderr, "coverage count nothing was holding: %s, this run compared %lu\n",
					     it->first.c_str(), (unsigned long) it->second);
				agree = false;
			}
			continue;
		}
		if (e->second == it->second)
			continue;

		/* Which way it moved, because the two mean different things and the
		   message that only says they differ is the one that gets answered by
		   moving the number. Fewer is either a scan that has stopped reading
		   the source or a screen or a row that was deliberately taken out.
		   More is a screen or a row that was added, or a scan that has started
		   reading something it did not read before. */
		std::fprintf(stderr, "coverage count %s: this run compared %lu where the file holds %lu, %s\n",
			     it->first.c_str(), (unsigned long) it->second, (unsigned long) e->second,
			     it->second < e->second
				     ? "so either the scan has stopped reading the source or what it read was taken out on purpose"
				     : "so either something was added or the scan has started reading more than it did");
		agree = false;
	}

	for (std::map<std::string, size_t>::const_iterator it = expected.begin();
	     whole && it != expected.end(); ++it)
	{
		if (measured().count(it->first) == 1)
			continue;
		std::fprintf(stderr, "coverage count %s is in the file and nothing measured it\n",
			     it->first.c_str());
		agree = false;
	}

	if (!agree && whole)
		std::fprintf(stderr,
			     "what this run measured has been written to %s;\n"
			     "  check each line that moved and put that file in place of %s\n",
			     actual_path, expected_path);
	return whole ? agree : true;
}
