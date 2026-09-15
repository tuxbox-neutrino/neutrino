/*
 * settingstable.h - the table binding settings names to fields
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

#ifndef __coreapi_settingstable_h__
#define __coreapi_settingstable_h__

#include "coreapi/base/schema.h"

namespace coreapi
{

/* Every setting this layer knows, declared once, so reading a setting, writing
   it and describing it are one declaration rather than three places that have to
   agree. The array and its count come as a pair from one place, because a count
   that outgrew its array reads a row that is not there. */
const Descriptor *settingsTable();
size_t settingsTableCount();

/* Lets a case hold this layer to a row the shipped table does not have. Without
   it a rule two implementations agree on over the shipped rows is one no case
   can tell apart: a Bool checked against its two values and one checked against
   a row's bounds behave alike while every Bool row declares nought and one.

   NULL puts the shipped table back. A seam for the cases and nothing else: what
   it swaps is read by every walk of the table, the source that locates a value
   in the program's own struct included, so a case that left a fixture installed
   would decide what the cases after it check. Restore it from a destructor
   rather than a last line, or a check that fails and unwinds leaves it
   standing. */
void setSettingsTable(const Descriptor *table, size_t count);

/* A setting the tables leave out on purpose, and why. Listed rather than left
   out silently, because a setting missing for no stated reason reads the same as
   one nobody noticed.

   The field is written by the macro a row writes, so the name is held to a
   member of the struct by the compiler. What holds the list to the struct is the
   check over the settings struct in the tests: the rows plus these are what the
   struct holds, and neither number is read from a document. */
struct UndeclaredSetting
{
	const char *reason;
	FieldRef    where;
};

const UndeclaredSetting *settingsUndeclared(size_t &count);

/* One accessor per section, each in a file of its own so that no single file
   carries the whole stock. settingsTable() answers with these joined.

   A row in one of those files states the key, the type and the default off the
   line that loads the setting, and the bounds, the choices and the labels off
   the screen that offers it, and cites both, because a bound that is nearly
   right reads as correct on the page and most of the numbers are ones no
   extractor holds to. None of the sections needs a restart: each setting is read
   again by whatever acts on it. A file says something here only where it departs
   from that. */
const Descriptor *settingsTableAudio(size_t &count);
const Descriptor *settingsTableOsd(size_t &count);
const Descriptor *settingsTableMisc(size_t &count);
const Descriptor *settingsTableVideo(size_t &count);
const Descriptor *settingsTableRecording(size_t &count);
const Descriptor *settingsTableChannel(size_t &count);
const Descriptor *settingsTableNetwork(size_t &count);
const Descriptor *settingsTableWeather(size_t &count);
const Descriptor *settingsTableKeys(size_t &count);
const Descriptor *settingsTableDisplay(size_t &count);
const Descriptor *settingsTablePlayer(size_t &count);
const Descriptor *settingsTableParental(size_t &count);
const Descriptor *settingsTableCam(size_t &count);
const Descriptor *settingsTableHdd(size_t &count);
const Descriptor *settingsTableUpdate(size_t &count);

} // namespace coreapi

#endif
