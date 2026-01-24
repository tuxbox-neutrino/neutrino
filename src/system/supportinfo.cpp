/*
	Neutrino-HD

	Copyright (C) 2025 Thilo Graf

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "supportinfo.h"
#include <global.h>
#include <fstream>
#include <string>

// Default URLs (compile-time fallbacks)
#define DEFAULT_DOCS_URL   "https://wiki.tuxbox-neutrino.org"
#define DEFAULT_FORUM_URL  "https://forum.tuxbox-neutrino.org"

// Config file paths
#define OS_RELEASE_FILE    "/etc/os-release"

CSupportInfo& CSupportInfo::getInstance()
{
	static CSupportInfo instance;
	return instance;
}

CSupportInfo::CSupportInfo()
{
	loadUrls();
}

void CSupportInfo::reload()
{
	loadUrls();
}

void CSupportInfo::loadUrls()
{
	// Set compile-time defaults
	homepage = PACKAGE_URL;
	docs = DEFAULT_DOCS_URL;
	forum = DEFAULT_FORUM_URL;

	// Helper lambda to strip quotes from os-release values
	auto stripQuotes = [](const std::string &s) -> std::string {
		if (s.length() >= 2 && s.front() == '"' && s.back() == '"')
			return s.substr(1, s.length() - 2);
		return s;
	};

	// Read from /etc/os-release (freedesktop.org standard format)
	// On modern images, /.version is a symlink to /etc/os-release
	std::ifstream osrelease(OS_RELEASE_FILE);
	if (osrelease.is_open())
	{
		std::string line;
		while (std::getline(osrelease, line))
		{
			if (line.compare(0, 9, "HOME_URL=") == 0)
				homepage = stripQuotes(line.substr(9));
			else if (line.compare(0, 18, "DOCUMENTATION_URL=") == 0)
				docs = stripQuotes(line.substr(18));
			else if (line.compare(0, 12, "SUPPORT_URL=") == 0)
				forum = stripQuotes(line.substr(12));
		}
		osrelease.close();
	}

	// Fallback: Read legacy IMAGE_VERSION_FILE fields
	// These override os-release values if present (for backward compatibility)
	std::ifstream versionfile(IMAGE_VERSION_FILE);
	if (versionfile.is_open())
	{
		std::string line;
		while (std::getline(versionfile, line))
		{
			if (line.compare(0, 9, "homepage=") == 0)
				homepage = line.substr(9);
			else if (line.compare(0, 5, "docs=") == 0)
				docs = line.substr(5);
			else if (line.compare(0, 6, "forum=") == 0)
				forum = line.substr(6);
		}
		versionfile.close();
	}
}
