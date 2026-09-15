/*
 * settings.h - reading and writing the box settings
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

#ifndef __coreapi_settings_h__
#define __coreapi_settings_h__

#include "coreapi/base/result.h"
#include "coreapi/base/schema.h"

#include <string>
#include <vector>

namespace coreapi
{
namespace settings
{

/* Answered out of the declaration and not out of the store, so a box with no
   store behind it still answers. A row the schema marks secret keeps its place
   and loses its declared default. */
Result<std::vector<Descriptor> > schema();

// Each section once, in the order the schema first names it, so a frontend can
// lay out its menu without walking the whole schema.
Result<std::vector<std::string> > sections();

// NotFound for a key nothing declares: a zeroed descriptor would be a lie a
// caller cannot tell from a setting that really has no label and no bounds. A
// secret row comes back with its default withheld.
Result<Descriptor> describe(const std::string &key);

/* The current value, rendered the way the wire carries it. A key the store has never held
   reads as what the row declares, that being what the program's own load does with one. A
   secret row answers the empty string and the store is not asked at all. What a write left
   held reads back before the box has taken it, so a caller that writes and reads is not
   told its own write did nothing; a write this layer answered with an error left nothing. */
Result<std::string> get(const std::string &key);

/* One setting, given the way the wire carries it, so what get() answered can be sent again
   unchanged. The value is held to three things and each refuses a value the next would not:
   what the settings file can carry, what the declaration allows, and what the field behind
   the row can hold.

   The first is what bounds a String. The program writes a setting as its key, a separator
   and the value, reads one back by splitting the line at the first separator and cutting the
   rest off at the first number sign, and escapes nothing either way. So a String may hold
   text of one line: no byte that ends a line and no other control byte, no zero byte, no
   number sign, no space at either end, and at most four kilobytes.

   A secret row refuses an empty value, so a form redrawn from a read that answered nothing
   cannot clear the credential; emptying one is clearSecret.

   ok says the value passed all three, the store took it, and the box was asked on its own
   loop to put it into the program's settings, save them and tell whoever applies that
   section. It does not say the file has been written or that anything has been applied: the
   loop answers nothing and waiting for it from here deadlocks.

   setting-not-written says the value is not in the box and will not get there, and what this
   call wrote is taken back rather than left for a later write of some other setting to carry
   in. What is taken back is this call's own write, so two requests at once do not undo each
   other. The one thing it cannot promise is that nothing landed. */
Result<void> set(const std::string &key, const std::string &value);

/* Empties a credential, the one thing set() will not do. The two rules that protect one
   leave no way to remove it: a read answers nothing, so a form redrawn from what it read
   offers nothing back, and a write of nothing is refused so that the round trip cannot wipe
   it.

   not-a-credential rather than no-such-setting for a key that is declared and is not one,
   because the key is right and telling a caller there is no such setting sends it looking
   for a name it already has. */
Result<void> clearSecret(const std::string &key);

/* Two kinds of row answer and they answer the same shape: a row carrying its own list
   answers it with each label already resolved to the text the box would show, and a row
   whose set is the box's own is asked for it, a list written into a table being right for
   one box model and quietly wrong for the next.

   choices-unavailable covers a setting that offers no set at all and one whose set is the
   box's own while nobody has said what it is. An empty list is not that answer and does not
   happen, so ok always carries at least one value. */
Result<std::vector<SettingChoice> > choices(const std::string &key);

/* The text a label_key names, in whatever language the box has loaded, through the seam in
   deps.h. False for a NULL key and for one the catalog carries nothing under, answered alike
   so that a caller may run this straight off either field without a NULL check. Neither ever
   hands the key's own spelling back in out, which is what a route that printed label_key
   itself did and is the whole reason this exists. */
bool resolveLabel(const char *key, std::string &out);

} // namespace settings
} // namespace coreapi

#endif
