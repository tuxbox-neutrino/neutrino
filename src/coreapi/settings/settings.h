/*
 * settings.h - reading and writing the box settings
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions)
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
/* Whether a text is one the rule takes: its length, its characters, and for a
   path the file or folder it names. An empty text is refused where the rule names
   a place, unless the rule says allow_empty. A value equal to current, when given,
   skips the tests about the place, so what is stored never has to be proven
   again. Touches the file system only for a rule that asks for an existing path. */
Result<void> holdsTextRule(const TextRule &rule, const std::string &value,
			   const std::string *current = NULL);

// Whether a folder on a file system of this type (the kernel's type number) is
// acceptable at the given level of MustExist.
bool fileSystemAllowed(MustExist level, long fs_type);

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

// Each setting a write could not keep, with the answer its own write would have had.
typedef std::vector<std::pair<std::string, Error> > Refusals;

/* What a write of several settings will leave behind, as key and wire text, so each of
   them is judged on that and not on what the store holds before any has landed. Only
   values check() passed and settleBatch() kept belong in it: a value that never lands
   must not be what allows another one. */
struct BatchOverlay
{
	std::vector<std::pair<std::string, std::string> > values;
};

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

   Written alone (after NULL), a value that passed all three is still refused with
   setting-condition-not-met where the row's conditions do not hold on the store, its
   depends_on naming the settings that refused it, and with not-a-listed-value where it names
   an entry whose own conditions do not hold. A value the store already holds passes them:
   writing back what a row holds is no change for the conditions to judge. A write of several
   hands in after, what the whole write leaves behind: settleBatch() has judged the conditions
   on it, so the order of the settings does not matter, and they are not judged again here;
   the entries a value may name are still read on after.

   ok says the value passed all three, the store took it, and the box was asked on its own
   loop to put it into the program's settings, save them and tell whoever applies that
   section. It does not say the file has been written or that anything has been applied: the
   loop answers nothing and waiting for it from here deadlocks.

   setting-not-written says the value is not in the box and will not get there, and what this
   call wrote is taken back rather than left for a later write of some other setting to carry
   in. What is taken back is this call's own write, so two requests at once do not undo each
   other. The one thing it cannot promise is that nothing landed. */
Result<void> set(const std::string &key, const std::string &value, const BatchOverlay *after = NULL);

/* Whether a row's entry is offered with the settings as they stand: the box has what it
   stands for and the settings its own conditions read allow it. */
bool entryOfferedNow(const EnumValue &e);

/* Whether the parental lock holds this row right now. Also true when whether the
   box is locked cannot be read, so a failed read never unlocks a row. Asks the
   box only for a row the lock can hold. */
bool lockedNow(const std::string &key);

/* Whether this box has what the row controls, asked of the box every time. A row
   the box lacks is still declared and still read, and every write of it is
   refused; one every box has is always there. */
bool availableNow(const Descriptor &d);

/* Every rule set() holds a value to, without the store: what set() would refuse, check()
   refuses with the same answer, and ok says set() would come as far as the store. For a
   write of several settings that must not land in part, which asks each of them first.
   The conditions are left out, the row's and those of the entry a value names: they are
   judged on what the whole write leaves behind, which settleBatch() asks once every member
   has passed here. */
Result<void> check(const std::string &key, const std::string &value);

/* Whether value is what the row holds now, the default for a key never stored, for a value
   nobody has checked yet, as in the source: one check() refuses is not the stored value, and
   no value of a credential counts, so a guess at one is not told apart. */
bool holdsStored(const std::string &key, const std::string &value);

/* The second pass of a write of several settings, between check() and set(). First runs the
   couplings over batch, which are the settings that cannot be written apart:
   - writing the guide's save on puts its read on, and the module line follows its position;
     a member the caller names with a value that contradicts that is refused together with
     the member that implies it, while an equal value is taken as sent;
   - the name and identifier of a start channel and the city and coordinates of the weather
     are written together or not at all, and a weather pair clears the postal code;
   - the five plugin lists stay one partition: a name written into one list leaves the
     others, and two lists written together naming one plugin are both refused.
   A coupling puts its values into batch, so a caller writes batch afterwards and not the
   members it named. All of them are refused with setting-condition-not-met although the
   schema states no condition for them: the answer names the pair or the clash.

   Then takes out of batch every member whose conditions do not hold on batch and the store,
   and again until a round takes out nothing, because a member taken out can be what another
   one leaned on. The couplings are made over from what is left each round, so what a member
   brought with it leaves with it, and a setting a coupling added that cannot land takes out
   the member that asked for it. Each one taken out, added settings too, is put into refused
   under its own key with setting-condition-not-met. What is left then holds together:
   written with set() and this batch, every member lands on conditions the landed members
   and the store satisfy, whatever order the caller named them in.

   Two things it does not promise. The outcome is not the largest set that could land: two
   members whose conditions each refuse the other's new value are both taken out, although
   either alone might have been allowed. And a store that fails to take a member in the
   write after this cannot be foreseen here, so a member may still land on one that did
   not.

   A member equal to the stored value is judged by no condition and is taken out of batch as
   done, not put into refused, so nothing is written, saved or applied for it. keepUnchanged
   leaves such members in and judges them, for a menu that has put the value into the
   program's settings itself and needs its key applied. */
void settleBatch(BatchOverlay &batch, Refusals &refused, bool keepUnchanged = false);

/* All three passes of a write of several settings: check() on each, settleBatch(), then set()
   on what is left. The one entry for a caller that writes more than one setting, so none can
   skip a coupling. Each key may be named once. Every setting that did not land, whichever pass
   refused it, is in failed with the answer its own write would have had, and what the
   couplings added is among them when it fails. */
void writeBatch(const std::vector<std::pair<std::string, std::string> > &members, Refusals &failed);

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

// Called once changed settings are saved, from a menu or a write alike.
void announceSettingsChanged();

} // namespace settings
} // namespace coreapi

#endif
