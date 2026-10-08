/*
 * choicesources.cpp - the lists of installed languages and zones a text setting offers
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

#include <config.h>

#include "coreapi/settings/choicesources.h"

#include <xmlinterface.h>

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace coreapi
{

namespace
{

/* Where the box keeps the zone files: the standard place, and the one older
   images used when that has none. The standard one is what is named when the
   zone is in neither. */
std::string zoneinfoPath(const std::string &root, const std::string &zone)
{
	const std::string standard = root + "/usr/share/zoneinfo/" + zone;
	if (!access(standard.c_str(), R_OK))
		return standard;

	const std::string legacy = root + "/share/zoneinfo/" + zone;
	if (!access(legacy.c_str(), R_OK))
		return legacy;

	return standard;
}

} // namespace

bool installedLocales(std::vector<SettingChoice> &out)
{
	std::vector<std::string> dirs;
	dirs.push_back(LOCALEDIR);
	dirs.push_back(LOCALEDIR_VAR);
	return localesIn(dirs, out);
}

bool localesIn(const std::vector<std::string> &dirs, std::vector<SettingChoice> &out)
{
	// A name in both directories is one catalog, so it is offered once.
	std::vector<SettingChoice> found;
	std::set<std::string> seen;

	for (size_t p = 0; p < dirs.size(); p++)
	{
		struct dirent **names = NULL;
		const int n = scandir(dirs[p].c_str(), &names, 0, alphasort);
		if (n < 0)
			continue;
		for (int i = 0; i < n; i++)
		{
			const std::string file(names[i]->d_name);
			free(names[i]);
			const size_t at = file.find(".locale");
			if (at == std::string::npos)
				continue;
			const std::string name = file.substr(0, at);
			if (!seen.insert(name).second)
				continue;
			SettingChoice one;
			one.text = name;
			one.label = name;
			found.push_back(one);
		}
		free(names);
	}

	if (found.empty())
		return false;
	out.swap(found);
	return true;
}

bool languageNames(std::vector<SettingChoice> &out)
{
	return languagesFrom(DATADIR "/iso-codes/iso-639.tab", out);
}

/* The table's lines are three codes and then the name, which may hold blanks; a line
   that starts a comment is skipped. The name is what the settings keep. */
bool languagesFrom(const std::string &table, std::vector<SettingChoice> &out)
{
	std::ifstream in(table.c_str());
	if (!in.is_open())
		return false;

	std::set<std::string> names;
	std::string line;
	while (std::getline(in, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		std::istringstream fields(line);
		std::string a, b, c, name;
		if (!(fields >> a >> b >> c >> std::ws))
			continue;
		std::getline(fields, name);
		while (!name.empty() && (name[name.size() - 1] == '\r' || name[name.size() - 1] == ' '))
			name.erase(name.size() - 1);
		if (!name.empty())
			names.insert(name);
	}
	if (names.empty())
		return false;

	std::vector<SettingChoice> found;
	SettingChoice none;
	none.text = "none";
	none.label = "none";
	found.push_back(none);
	for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
	{
		SettingChoice one;
		one.text = *it;
		one.label = *it;
		found.push_back(one);
	}
	out.swap(found);
	return true;
}

bool timezoneNames(std::vector<SettingChoice> &out)
{
	return timezonesFrom("/etc/timezone.xml", TARGET_PREFIX, out);
}

bool timezonesFrom(const std::string &list, const std::string &root, std::vector<SettingChoice> &out)
{
	xmlDocPtr parser = parseXmlFile(list.c_str());
	if (parser == NULL)
		return false;

	std::vector<SettingChoice> found;
	xmlNodePtr search = xmlChildrenNode(xmlDocGetRootElement(parser));
	while (search)
	{
		if (!strcmp(xmlGetName(search), "zone"))
		{
			const char *zone = xmlGetAttribute(search, "zone");
			const char *name = xmlGetAttribute(search, "name");
			// A zone whose file is not installed cannot be linked to, so it is not offered.
			if (name != NULL && zone != NULL && !access(zoneinfoPath(root, zone).c_str(), R_OK))
			{
				SettingChoice one;
				one.text = name;
				one.label = name;
				found.push_back(one);
			}
		}
		search = xmlNextNode(search);
	}
	xmlFreeDoc(parser);

	if (found.empty())
		return false;
	out.swap(found);
	return true;
}

bool interfaceNames(std::vector<SettingChoice> &out)
{
	return interfacesIn("/sys/class/net", out);
}

bool interfacesIn(const std::string &dir, std::vector<SettingChoice> &out)
{
	struct dirent **names = NULL;
	const int n = scandir(dir.c_str(), &names, 0, alphasort);
	if (n < 0)
		return false;

	// What the network screen leaves out: the loopback and the entries of the directory itself.
	std::vector<SettingChoice> found;
	for (int i = 0; i < n; i++)
	{
		const std::string name(names[i]->d_name);
		free(names[i]);
		if (name.empty() || name[0] == '.' || name.compare(0, 2, "lo") == 0)
			continue;
		SettingChoice one;
		one.text = name;
		one.label = name;
		found.push_back(one);
	}
	free(names);

	if (found.empty())
		return false;
	out.swap(found);
	return true;
}

} // namespace coreapi
