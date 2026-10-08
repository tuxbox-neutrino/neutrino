/*
 * choicesources.h - the lists of installed languages and zones a text setting offers
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

#ifndef __coreapi_choicesources_h__
#define __coreapi_choicesources_h__

#include "coreapi/base/schema.h"

#include <string>
#include <vector>

namespace coreapi
{

/* The values a text setting offers right now, each as its stored text in
   SettingChoice::text, with value 0 and the text again as its label: what such a
   setting holds is the text, and a number has no place in it. Each can be the
   ChoiceSource of a row; languageNames is no row's yet, it is there for the
   preferred language rows. False where the box has nothing to offer, and out is
   then untouched. */

// The names of the language catalogs installed, in order, each once.
bool installedLocales(std::vector<SettingChoice> &out);

/* "none" and then the language names of the box's ISO 639 table, each once and in the
   alphabet, which is what a preferred audio language or subtitle language is stored as. */
bool languageNames(std::vector<SettingChoice> &out);
bool languagesFrom(const std::string &table, std::vector<SettingChoice> &out);

// The names of the time zones the box lists whose zone file is installed, in the box's order.
bool timezoneNames(std::vector<SettingChoice> &out);

/* The same two for any place the lists are kept, which is what the two above ask
   with the box's own and what a case asks with a fixture's: the names of the
   catalogs in the directories, and the zones of the list in the file whose zone
   files are installed under root (the box's prefix). */
bool localesIn(const std::vector<std::string> &dirs, std::vector<SettingChoice> &out);
bool timezonesFrom(const std::string &list, const std::string &root, std::vector<SettingChoice> &out);

/* The network interfaces the box has, by name and in the alphabet, as the network
   screen offers them: what /sys/class/net lists but the loopback. interfacesIn
   reads any such directory, which is what a case asks with a fixture's. */
bool interfaceNames(std::vector<SettingChoice> &out);
bool interfacesIn(const std::string &dir, std::vector<SettingChoice> &out);

} // namespace coreapi

#endif
