/*
 * settings.cpp - reading and writing the box settings
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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

#include "settings.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/eventbus.h"
#include "coreapi/settings/couple.h"
#include "settingstable.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <vector>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <sys/stat.h>
#include <sys/statfs.h>

namespace coreapi
{
namespace settings
{

namespace
{

// Linear over a few hundred rows, for the reason the store's own lookup is:
// what an index would save is less than building it costs for a request that
// reads a handful of settings.
const Descriptor *row(const std::string &key)
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();
	for (size_t i = 0; i < n; ++i)
	{
		if (t[i].key != NULL && key == t[i].key)
			return &t[i];
	}
	return NULL;
}

std::string decimal(long value)
{
	char out[32];
	snprintf(out, sizeof(out), "%ld", value);
	return std::string(out);
}

// The kinds whose value travels as text: the rest are numbers, a key code too.
bool holdsText(const Descriptor &d)
{
	return d.type == ValueType::String;
}

/* Strict, because the whole of the value is what was offered: a parse that
   stopped at the first character it did not like would take "1x" as 1 and report
   a setting nobody asked for as set. */
bool parseNumber(const std::string &v, long &out)
{
	if (v.empty())
		return false;
	errno = 0;
	char *end = NULL;
	long n = strtol(v.c_str(), &end, 10);
	if (errno != 0 || end == v.c_str() || *end != '\0')
		return false;
	out = n;
	return true;
}

/* What the settings file can carry on one line. The struct holds a value as text
   of no length in particular and the file has no length either, so this ceiling
   is one this layer picks: the longest value any row holds is a path. */
const size_t kMaxValueBytes = 4096;

/* What a value may be whatever kind the row is. A zero byte is the end of the
   string to every call under this, so a value carrying one is a different value
   from the one that was offered. */
Result<void> allowedAtAll(const std::string &value)
{
	if (value.find('\0') != std::string::npos)
		return fail(Status::InvalidArgument, ErrorCode::ValueHasZeroByte,
			    "the value carries a zero byte, which the calls below it would read as its end");
	if (value.size() > kMaxValueBytes)
		return fail(Status::InvalidArgument, ErrorCode::ValueTooLong,
			    "the value is longer than " + decimal((long) kMaxValueBytes) + " bytes");
	return ok();
}

/* What text may be, which is what one line of the settings file gives back
   unchanged. The program writes a setting as its key, a separator and the value,
   ends the line there, and reads it back by splitting at the first separator and
   cutting the rest off at the first number sign. Neither side escapes anything,
   so three kinds of byte do not survive the round trip:

   the byte that ends a line ends the value, and what follows it is read as a
   setting of its own under whatever key it names, which is any key the program
   loads, both pin rows and every credential among them;

   a number sign takes the rest of the line with it;

   a space at either end is written and read back while nothing that shows the
   value can show it, so two settings that differ look the same.

   The other control bytes go with the first. The separator needs no rule of its
   own: the split is at the first one, so a later one is part of the value. */
Result<void> allowedAsText(const std::string &value)
{
	for (size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char c = (unsigned char) value[i];
		if (c < 0x20 || c == 0x7f)
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes one line of text and no control byte in it");
		if (c == '#')
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting cannot hold a number sign, which is where the value is cut off when it is read back");
	}

	if (!value.empty() && (value[0] == ' ' || value[value.size() - 1] == ' '))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting does not keep a space at either end of its value");

	return ok();
}

/* A secret row answers no value of its own, and its declared default is one: the
   tables state the default the program falls back to because a check holds them
   to it, so the withholding is here rather than there. */
void withhold(Descriptor &d)
{
	if (!d.secret)
		return;
	d.default_string = "";
	d.default_int = 0;
	d.default_fn = NULL;
}

/* Whether the row allows the value, which is not the question the store asks.
   That one is whether the field can hold it, and it is narrower. Both have to
   refuse what they refuse and neither stands in for the other, because a row's
   bounds can be wider than its field and a field is wider than most rows'
   bounds. */
Result<void> allowedByRow(const Descriptor &d, long value, const ValueLookup *now)
{
	switch (d.type)
	{
		case ValueType::Bool:
			/* The two values the type itself has, rather than the row's
			   bounds: nothing holds a Bool row to declaring any, and the rows
			   of other kinds beside it leave both at zero. */
			if (value != 0 && value != 1)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes 0 or 1");
			return ok();

		case ValueType::Int:
		{
			// Every value the row names in words is taken beside the bounds.
			std::string also;
			for (size_t i = 0; d.values != NULL && i < d.value_count; ++i)
			{
				if (value == d.values[i].value)
					return ok();
				also += (i == 0 ? " or " : ", ") + decimal(d.values[i].value);
			}
			const Bounds b = boundsNow(d, now);
			if (value < b.min || value > b.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes " + decimal(b.min) + " to " + decimal(b.max) + also);
			return ok();
		}

		case ValueType::Enum:
			// Held to what the row offers by the caller, which asks the one function
			// that also leaves out what the box lacks.
			return ok();

		case ValueType::Key:
		{
			const Bounds b = boundsNow(d, now);
			if (value < b.min || value > b.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes the code of a key of the remote control");
			// Inside the bounds is still no key where the code is one the input layer
			// never delivers, such as a release.
			if (!keySource().known(value))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting takes the code of a key a remote control can send, or no key");
			return ok();
		}

		case ValueType::String:
			break;
	}

	return fail(Status::Internal, ErrorCode::BadTable,
		    "the setting is not of a kind a number is offered for");
}

/* The default and the stored value of a number row: writing either again is no new pick,
   whatever the row offers now. The store is read only when the default does not match. */
bool keptValue(const Descriptor &d, long number)
{
	if (number == defaultInt(d))
		return true;
	long stored = 0;
	return settingsSource().readInt(d.key, stored) == Status::Ok && stored == number;
}

void saidNameless(const char *key)
{
	static std::mutex lock;
	static std::set<std::string> told;
	std::lock_guard<std::mutex> hold(lock);
	if (told.insert(key).second)
		fprintf(stderr, "coreapi: the list of %s has entries without text and they are left out\n", key);
}

/* The list a row takes from its provider. False where the provider cannot say and
   where it says nothing, since a list of no entry offers nothing to choose and the two
   are answered alike. */
bool providedChoices(const Descriptor &d, std::vector<SettingChoice> &out)
{
	std::vector<SettingChoice> listed;
	if (!d.choices_from(listed) || listed.empty())
		return false;

	std::vector<SettingChoice> kept;
	kept.reserve(listed.size());
	bool nameless = false;
	// A number outside the row's range is one a write refuses, so it is not offered either.
	const Bounds range = boundsNow(d);
	for (size_t i = 0; i < listed.size(); ++i)
	{
		SettingChoice &one = listed[i];
		if (d.type == ValueType::Int && (one.value < range.min || one.value > range.max))
			continue;
		/* A String entry stands for its text. One without it is a provider that filled the
		   number and the label like a number row's, and every write would be refused for it
		   without a word, so it is left out and said once. */
		if (d.type == ValueType::String && one.text.empty())
		{
			nameless = true;
			continue;
		}
		std::string words;
		if (!one.label_key.empty() && resolveLabel(one.label_key.c_str(), words))
			one.label = words;
		else if (one.label.empty() && d.type == ValueType::String)
			one.label = one.text;
		kept.push_back(one);
	}
	if (nameless)
		saidNameless(d.key);
	if (kept.empty())
		return false;
	out.swap(kept);
	return true;
}

Result<void> holdsOffered(ChoiceSource from, const std::string &text, const std::string *always)
{
	// A value the caller knows the row holds, such as its default, needs no entry.
	if (always != NULL && *always == text)
		return ok();

	std::vector<SettingChoice> listed;
	// A box that cannot say what it has holds the row to nothing, as a rule of text would.
	if (!from(listed))
		return ok();

	/* An entry without text stands for nothing a text row can store, as in the list a read
	   answers, so it neither matches nor counts as an offer. */
	bool offered = false;
	for (size_t i = 0; i < listed.size(); ++i)
	{
		if (listed[i].text.empty())
			continue;
		offered = true;
		if (listed[i].text == text)
			return ok();
	}
	if (!offered)
		return ok();
	return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
		    "the setting does not offer that value");
}


ValueLookup lookupAfter(const BatchOverlay *batch);

/* The values a row offers, whichever of the places they come from. One
   function, because the write is held to the same set a read answers with:
   written twice, a caller could be offered a value the write turns down. False is
   a row that offers no set at all and a set nobody can state. The entries' own
   conditions are judged on batch first and the store after it;
   ignoreWhen leaves them out, for the pass that has no batch yet. */
bool valuesOffered(const Descriptor &d, std::vector<SettingChoice> &out,
		   const BatchOverlay *batch = NULL, bool ignoreWhen = false)
{
	const ValueLookup now = lookupAfter(batch);
	if (d.choices_from != NULL)
		return providedChoices(d, out);

	if (d.type != ValueType::Enum)
		return false;

	if (d.field.choices != NULL)
	{
		std::vector<SettingChoice> asked;
		if (settingChoices().values(d.field.choices, asked) != Status::Ok)
			return false;
		// A set nobody offers a value for is a row nothing can be drawn from,
		// which is the answer above rather than an empty list.
		if (asked.empty())
			return false;
		out.swap(asked);
		return true;
	}

	if (d.values == NULL || d.value_count == 0)
		return false;

	std::vector<SettingChoice> listed;
	listed.reserve(d.value_count);
	for (size_t i = 0; i < d.value_count; ++i)
	{
		const EnumValue &e = d.values[i];
		if (ignoreWhen ? (e.available != NULL && !e.available()) : !entryOffered(e, now))
			continue;
		SettingChoice one;
		one.value = e.value;
		if (e.label_text != NULL)
			one.label = e.label_text;
		else
		{
			/* The text and not the name of it, because that is what the other kind
			   answers with and a caller must not have to tell the two apart. A name
			   the catalog carries nothing under leaves the text empty. */
			if (e.label_key != NULL)
				one.label_key = e.label_key;
			resolveLabel(e.label_key, one.label);
		}
		listed.push_back(one);
	}
	if (listed.empty())
		return false;
	out.swap(listed);
	return true;
}

// The value batch writes under key, NULL when it writes none.
const std::string *batchValue(const BatchOverlay *batch, const Descriptor &row)
{
	if (batch == NULL)
		return NULL;
	for (size_t i = 0; i < batch->values.size(); ++i)
	{
		if (batch->values[i].first == row.key)
			return &batch->values[i].second;
	}
	return NULL;
}

/* The two readers a condition is judged with. False is "cannot say", which the evaluator
   takes as holding, so a store that fails to read never refuses a write. A credential is
   read from the store itself and not through get(), which withholds it: what a caller
   cannot see must not be what refuses its write. */
bool readNumberAfterBatch(const char *key, long *value, void *context)
{
	const Descriptor *row = settings::row(key);
	if (row == NULL || holdsText(*row))
		return false;

	const std::string *sent = batchValue(static_cast<const BatchOverlay *>(context), *row);
	if (sent != NULL)
		return parseNumber(*sent, *value);

	long stored = 0;
	Status s = settingsSource().readInt(row->key, stored);
	if (s == Status::NotFound)
		stored = defaultInt(*row);
	else if (s != Status::Ok)
		return false;
	*value = stored;
	return true;
}

bool readTextAfterBatch(const char *key, std::string *value, void *context)
{
	const Descriptor *row = settings::row(key);
	if (row == NULL || !holdsText(*row))
		return false;

	const std::string *sent = batchValue(static_cast<const BatchOverlay *>(context), *row);
	if (sent != NULL)
	{
		*value = *sent;
		return true;
	}

	std::string stored;
	Status s = settingsSource().readString(row->key, stored);
	if (s == Status::NotFound)
		stored = row->default_string != NULL ? row->default_string : "";
	else if (s != Status::Ok)
		return false;
	value->swap(stored);
	return true;
}

ValueLookup lookupAfter(const BatchOverlay *batch)
{
	// The cast only fits the lookup's untyped context; both readers take it back as const.
	ValueLookup after_batch = { readNumberAfterBatch, readTextAfterBatch,
	                            const_cast<BatchOverlay *>(batch) };
	return after_batch;
}

/* Asked last, once the value has passed every rule of its own: a value no row could
   take is wrong whatever the other settings hold, and that answer is the one a caller
   can act on without changing anything else. The entry of an enum that the value names
   is judged here too, on the batch and then the store, because the settings its own
   condition reads can be written in the same batch. value is NULL where there is none. */
void addOnce(std::vector<std::string> &keys, const char *key)
{
	if (key != NULL && std::find(keys.begin(), keys.end(), key) == keys.end())
		keys.push_back(key);
}

// The settings read by the conditions of d that do not hold, a group that fails naming all it reads.
std::vector<std::string> failingKeys(const Descriptor &d, const ValueLookup &lookup)
{
	std::vector<std::string> keys;
	for (size_t i = 0; d.conditions != NULL && i < d.condition_count; ++i)
	{
		const Condition &c = d.conditions[i];
		if (conditionsHold(&c, 1, lookup))
			continue;
		if (!conditionIsGroup(c))
			addOnce(keys, c.key);
		for (size_t m = 0; conditionIsGroup(c) && c.any_of != NULL && m < c.any_count; ++m)
			addOnce(keys, c.any_of[m].key);
	}
	return keys;
}

Result<void> allowedByConditions(const Descriptor &d, const BatchOverlay *batch, const std::string *value = NULL)
{
	const ValueLookup after_batch = lookupAfter(batch);
	/* The source leaves a pair to its couplings where the row's conditions read only its
	   partner. No row of this tree has such conditions (the start channel and the weather
	   place read uselastchannel and weather_enabled), and its couplings judge none, so
	   every row is judged by its own conditions here. */
	if (!conditionsHold(d, after_batch))
		return Result<void>::failure(conditionRefusal("the settings this one depends on do not allow it to be set",
							      failingKeys(d, after_batch)));

	long number = 0;
	if (value != NULL && d.type == ValueType::Enum && d.choices_from == NULL && d.values != NULL &&
	    parseNumber(*value, number))
	{
		for (size_t i = 0; i < d.value_count; ++i)
		{
			const EnumValue &e = d.values[i];
			if (e.value == number && e.when_count > 0 && !conditionsHold(e.when, e.when_count, after_batch) &&
			    !keptValue(d, number))
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting does not offer that value");
		}
	}
	return ok();
}

/* The file system types the recording and update screens refuse a folder on: flash,
   which wears out under a recording, and memory, which a restart empties. */
const long kFsRamfs = 0x858458f6L;
const long kFsTmpfs = 0x1021994L;
const long kFsJffs2 = 0x72b6L;

bool endsWithNoCase(const std::string &name, const std::string &ending)
{
	if (name.size() < ending.size())
		return false;
	const size_t at = name.size() - ending.size();
	for (size_t i = 0; i < ending.size(); ++i)
	{
		if (tolower((unsigned char) name[at + i]) != tolower((unsigned char) ending[i]))
			return false;
	}
	return true;
}

bool hasExtension(const std::string &name, const char *list)
{
	std::string rest = list;
	while (!rest.empty())
	{
		const size_t comma = rest.find(',');
		const std::string one = rest.substr(0, comma);
		if (!one.empty() && endsWithNoCase(name, "." + one))
			return true;
		if (comma == std::string::npos)
			break;
		rest = rest.substr(comma + 1);
	}
	return false;
}

} // anonymous namespace

bool fileSystemAllowed(MustExist level, long fs_type)
{
	const bool memory = fs_type == kFsRamfs || fs_type == kFsTmpfs;
	if (fs_type == kFsJffs2)
		return level == MustExist::No || level == MustExist::Yes;
	return !(memory && level == MustExist::YesNotTmpfs);
}

namespace
{

/* The folder test the screens made through the folder browser: it exists, and the
   file system under it is one the rule allows. A failed statfs refuses, as the
   screen's own test did. */
Result<void> existsAsRequired(const TextRule &rule, const std::string &value)
{
	struct stat st;
	if (stat(value.c_str(), &st) != 0)
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a place that does not exist on the box");

	if (rule.kind == TextKind::Directory && !S_ISDIR(st.st_mode))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a folder and this is not one");
	if (rule.kind == TextKind::File && !S_ISREG(st.st_mode))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a file and this is not one");

	if (rule.must_exist == MustExist::Yes)
		return ok();

	struct statfs fs;
	if (statfs(value.c_str(), &fs) != 0)
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the file system under the setting's place cannot be told");
	if (!fileSystemAllowed(rule.must_exist, (long) fs.f_type))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting cannot be on this kind of file system");
	return ok();
}

} // anonymous namespace

Result<void> holdsTextRule(const TextRule &rule, const std::string &value, const std::string *current)
{
	switch (textFault(rule, value))
	{
		case TextFault::None:
			break;
		case TextFault::TooShort:
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes at least " + decimal((long) rule.min_length) + " characters");
		case TextFault::TooLong:
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "the setting takes at most " + decimal((long) rule.max_length) + " characters");
		case TextFault::BadCharacter:
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    std::string("the setting takes only these characters: ") + rule.allowed);
	}

	/* What is already stored is not asked about again, so a client that sends the
	   whole section back for an edit elsewhere is not refused for a disk that is
	   not mounted right now. Only a changed value has to name something there. */
	if (current != NULL && *current == value)
		return ok();

	if (value.empty())
	{
		if (rule.allow_empty || (rule.must_exist == MustExist::No && rule.extensions == NULL))
			return ok();
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "the setting names a place and cannot be empty");
	}

	if (rule.extensions != NULL && !hasExtension(value, rule.extensions))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    std::string("the setting takes a file ending in one of: ") + rule.extensions);

	if (rule.must_exist != MustExist::No)
		return existsAsRequired(rule, value);
	return ok();
}

Result<std::vector<Descriptor> > schema()
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();
	std::vector<Descriptor> out;
	if (t != NULL)
		out.assign(t, t + n);
	for (size_t i = 0; i < out.size(); ++i)
		withhold(out[i]);
	return ok(std::move(out));
}

Result<std::vector<std::string> > sections()
{
	const Descriptor *t = settingsTable();
	const size_t n = settingsTableCount();

	std::vector<std::string> out;
	for (size_t i = 0; i < n; ++i)
	{
		if (t[i].section == NULL)
			continue;
		bool seen = false;
		for (size_t j = 0; j < out.size() && !seen; ++j)
			seen = out[j] == t[i].section;
		if (!seen)
			out.push_back(t[i].section);
	}
	return ok(std::move(out));
}

Result<Descriptor> describe(const std::string &key)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");
	Descriptor out = *d;
	withhold(out);
	return ok(out);
}

Result<std::string> get(const std::string &key)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	/* A credential is answered with nothing rather than with what is stored, and the store is
	   not asked at all. Empty and not an error: the schema says the row is secret, while an
	   error could not be told from a store that failed and would leave a frontend unable to
	   draw the field at all. */
	if (d->secret)
		return ok(std::string());

	if (d->type == ValueType::String)
	{
		std::string value;
		Status s = settingsSource().readString(d->key, value);
		// A store that never held the key answers what the row declares,
		// which is what the program's own load puts there.
		if (s == Status::NotFound)
			return ok(std::string(d->default_string != NULL ? d->default_string : ""));
		if (s != Status::Ok)
			return fail(s, ErrorCode::SettingUnreadable,
				    "the setting could not be read");
		return ok(std::move(value));
	}

	long value = 0;
	Status s = settingsSource().readInt(d->key, value);
	if (s == Status::NotFound)
		value = defaultInt(*d);
	else if (s != Status::Ok)
		return fail(s, ErrorCode::SettingUnreadable,
			    "the setting could not be read");
	return ok(decimal(value));
}

namespace
{

/* Every rule set() holds a value to before the store, with the number a number row
   stores in number. Shared by set() and check(), so the two cannot come to disagree. */
Result<void> heldToRow(const Descriptor *d, const std::string &value, long &number,
		       const BatchOverlay *batch = NULL, bool ignoreWhen = false)
{
	// Ahead of every rule about the value: a row the lock fixes takes none.
	if (lockedNow(d->key))
		return fail(Status::Conflict, ErrorCode::SettingLocked,
			    "the box's parental lock fixes this setting");
	// Refused where the box lacks what the setting controls.
	if (!availableNow(*d))
		return fail(Status::Conflict, ErrorCode::SettingNotOnThisBox,
			    "this box does not have what the setting controls");

	/* A credential reads as nothing, so a form that redraws itself from what it read offers
	   nothing back here, and taking that would wipe the value the read protected. Refused for
	   every kind rather than only for text, so the answer says why: a number row would
	   otherwise turn an emptied field down as a bad number. */
	if (d->secret && value.empty())
		return fail(Status::InvalidArgument, ErrorCode::EmptyCredential,
			    "the setting is a credential and is not cleared by writing nothing");

	// The store is not touched until the value has passed the row, so a request
	// refused anywhere here leaves every setting as it was.
	Result<void> shape = allowedAtAll(value);
	if (!shape.ok())
		return fail(shape.error());

	if (d->type == ValueType::String)
	{
		Result<void> text = allowedAsText(value);
		if (!text.ok())
			return fail(text.error());

		if (d->text != NULL)
		{
			/* What is stored passes again however the place is mounted now. A rule that
			   looks for the place reads it first: asked only after a refusal, a folder on
			   a disk that does not answer would hold the write up in that look. Every other
			   rule asks the store only for a value it refused. */
			std::string stored;
			const bool looks = d->text->must_exist != MustExist::No;
			const bool have_stored = looks && settingsSource().readString(d->key, stored) == Status::Ok;
			Result<void> rule = holdsTextRule(*d->text, value, have_stored ? &stored : NULL);
			if (!rule.ok() && !looks && settingsSource().readString(d->key, stored) == Status::Ok)
				rule = holdsTextRule(*d->text, value, &stored);
			if (!rule.ok())
				return fail(rule.error());
		}

		/* An identifier is text here and a number where the program keeps it, so the one
		   spelling that survives is the one a channel is named by everywhere else. A value the
		   store cannot read would travel as far as the field, be dropped there and read back as
		   whatever the field already held, on a thread with nobody left to answer. */
		if (d->field.origin == FieldOrigin::ChannelIdField)
		{
			unsigned long long id = 0;
			if (!readChannelIdText(value, id))
				return fail(Status::InvalidArgument, ErrorCode::BadString,
					    "the setting takes the hexadecimal spelling a channel is named by, of one to sixteen digits");
		}

		if (d->choices_from != NULL)
		{
			/* The default counts as offered: a row reads as its default until something is
			   stored, so refusing it would make the way back to it a write that fails. The
			   stored value is read only once the list has refused. */
			const std::string fallback = d->default_string != NULL ? d->default_string : "";
			Result<void> offered = holdsOffered(d->choices_from, value, &fallback);
			if (!offered.ok())
			{
				std::string stored;
				if (settingsSource().readString(d->key, stored) == Status::Ok && stored == value)
					offered = ok();
			}
			if (!offered.ok())
				return fail(offered.error());
		}
		return ok();
	}

	if (!parseNumber(value, number))
		return fail(Status::InvalidArgument, ErrorCode::NotANumber,
			    "the setting takes a whole number");

	Result<void> allowed = allowedByRow(*d, number, NULL);
	if (!allowed.ok() && boundsVary(*d) && number >= d->min && number <= d->max)
	{
		/* Inside the constants and outside what the box states now: the default is the
		   way back and the stored value is what a bound that has moved since leaves
		   behind, so writing either again is no new pick. The stored value is read only
		   on a refusal. */
		long stored = 0;
		if (number == defaultInt(*d) ||
		    (settingsSource().readInt(d->key, stored) == Status::Ok && stored == number))
			allowed = ok();
	}
	if (!allowed.ok())
		return fail(allowed.error());

	/* Held to what the box offers, which leaves out the entries it lacks. While the box
	   cannot say what it has, only the default and the stored value pass: any other
	   could be a number the box cannot show. */
	if (d->type == ValueType::Enum)
	{
		std::vector<SettingChoice> offered;
		bool listed = false;
		const bool stated = valuesOffered(*d, offered, batch, ignoreWhen);
		for (size_t i = 0; stated && !listed && i < offered.size(); ++i)
			listed = offered[i].value == number;
		/* The default and the stored value pass again, as for a number: what a box
		   that lacks the entry now falls back to or still holds is the way back. */
		if (!listed && !keptValue(*d, number))
		{
			if (!stated && d->field.choices != NULL)
				return fail(Status::InvalidArgument, ErrorCode::ChoicesUnavailable,
					    "the values this setting offers are the box's own and nothing has said what they are");
			return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
				    "the setting does not offer that value");
		}
	}
	else if (d->choices_from != NULL)
	{
		/* Held to the list a read answers, so the two cannot disagree. A list the box
		   cannot give, and one with no entry the range takes, leave the range alone to
		   hold, as the read then offers a free number. The default and the stored value
		   pass, the store read only on a refusal. */
		std::vector<SettingChoice> offered;
		bool listed = !valuesOffered(*d, offered, batch, ignoreWhen);
		for (size_t i = 0; !listed && i < offered.size(); ++i)
			listed = offered[i].value == number;
		if (!listed && !keptValue(*d, number))
			return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
				    "the setting does not offer that value");
	}

	return ok();
}

/* Whether value is what the row holds now, the default for a key the store has never
   held. A value the store cannot be asked about is no match, and neither is any value of a
   credential: answering ok to a guess that matched would tell it apart from one that did
   not. */
bool sameAsStored(const Descriptor &d, const std::string &value)
{
	if (d.secret)
		return false;
	if (holdsText(d))
	{
		std::string stored;
		const Status s = settingsSource().readString(d.key, stored);
		if (s == Status::NotFound)
			stored = d.default_string != NULL ? d.default_string : "";
		else if (s != Status::Ok)
			return false;
		return stored == value;
	}
	long number = 0;
	if (!parseNumber(value, number))
		return false;
	long stored = 0;
	const Status s = settingsSource().readInt(d.key, stored);
	if (s == Status::NotFound)
		stored = defaultInt(d);
	else if (s != Status::Ok)
		return false;
	return stored == number;
}

} // anonymous namespace

bool entryOfferedNow(const EnumValue &e)
{
	return entryOffered(e, lookupAfter(NULL));
}

Result<void> conditionsAllow(const std::string &key, const std::string &value, const BatchOverlay &after)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");
	/* As in the source: asked, and the answer dropped for a value the store holds, since
	   writing back what a row holds is no change for the conditions to judge, and a client
	   that sends a whole section back must not be refused for a row it did not touch. */
	Result<void> allowed = allowedByConditions(*d, &after, &value);
	if (!allowed.ok() && sameAsStored(*d, value))
		return ok();
	return allowed;
}

bool lockedNow(const std::string &key)
{
	if (!heldByParentalLock(key.c_str()))
		return false;
	bool locked = true;
	if (!systemSourceInstalled() || systemSource().parentalLocked(locked) != Status::Ok)
		return true;
	return locked;
}

bool availableNow(const Descriptor &d)
{
	return d.field.available == NULL || d.field.available();
}

Result<void> check(const std::string &key, const std::string &value)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");
	long number = 0;
	// The entries' own conditions wait for the batch, which the first pass of a batch has not got.
	return heldToRow(d, value, number, NULL, true);
}

Result<void> set(const std::string &key, const std::string &value, const BatchOverlay *after)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	long number = 0;
	Result<void> held = heldToRow(d, value, number, after);
	if (!held.ok())
		return held;

	// Last, as in the source: a value the row refuses is wrong whatever the others hold.
	Result<void> allowed = allowedByConditions(*d, after, &value);
	if (!allowed.ok() && !sameAsStored(*d, value))
		return allowed;

	const Status written = d->type == ValueType::String ? settingsSource().writeString(d->key, value)
	                                                    : settingsSource().writeInt(d->key, number);
	if (written != Status::Ok)
		return fail(written, ErrorCode::SettingNotWritten,
			    "the setting could not be written");

	/* Saved here rather than left to the caller: a value the store took and
	   nobody saved is gone the next time the program writes its file. */
	Status saved = settingsSource().persist();
	if (saved != Status::Ok)
		return fail(saved, ErrorCode::SettingNotWritten,
			    "the setting was taken and not saved");

	/* Nothing is applied from here. The value is not in the program's own settings yet: the
	   store holds it and the save above is what carries it there, so whoever applies the change
	   is asked by whatever carries the write. An applier run here would read the value the box
	   was running on before. */
	return ok();
}

namespace
{

bool alreadyRefused(const Refusals &refused, const std::string &key)
{
	for (size_t i = 0; i < refused.size(); ++i)
	{
		if (refused[i].first == key)
			return true;
	}
	return false;
}

bool eraseMember(BatchOverlay &batch, const std::string &key)
{
	for (size_t i = 0; i < batch.values.size(); ++i)
	{
		if (batch.values[i].first == key)
		{
			batch.values.erase(batch.values.begin() + i);
			return true;
		}
	}
	return false;
}

void refuseByCondition(Refusals &refused, const std::string &key, const char *message)
{
	if (!alreadyRefused(refused, key))
		refused.push_back(std::make_pair(key,
			Error(Status::Conflict, ErrorCode::SettingConditionNotMet, message)));
}

} // anonymous namespace

void settleBatch(BatchOverlay &batch, Refusals &refused)
{
	/* What the caller named is kept apart from what the couplings made of it. Every round
	   makes the couplings over again from the survivors, so a member that fell takes what it
	   brought with it, and an addition that cannot land takes the member that brought it. The
	   members only ever leave, so this ends. */
	BatchOverlay named = batch;

	/* A round that takes nothing out ends the loop, and every other round takes out a member of
	   what the caller named, so the rounds cannot outnumber the members. The bound is that with
	   room for every coupling to refuse in its own round. It is here for a coupling that comes
	   to feed itself: that ends in a refusal of everything the caller named, never in a hang. */
	const size_t bound = (named.values.size() + 1) * (couplingCount() + 1);
	for (size_t round = 0;; ++round)
	{
		if (round > bound)
		{
			for (size_t i = 0; i < named.values.size(); ++i)
				refuseByCondition(refused, named.values[i].first,
					"the settings written together could not be settled");
			batch.values.clear();
			return;
		}

		BatchOverlay made = named;
		Refusals refusedByCouple;
		std::vector<Addition> added;
		applyCouplings(made, refusedByCouple, added);
		if (!refusedByCouple.empty())
		{
			// A refusal leaves the round half made, so it is made again without them.
			for (size_t i = 0; i < refusedByCouple.size(); ++i)
			{
				eraseMember(named, refusedByCouple[i].first);
				if (!alreadyRefused(refused, refusedByCouple[i].first))
					refused.push_back(refusedByCouple[i]);
			}
			continue;
		}

		/* Judged round by round against what is still in, because dropping one member can take
		   away what another one leaned on. */
		std::vector<std::string> failing;
		for (size_t i = 0; i < made.values.size(); ++i)
		{
			const Descriptor *row = findRow(made.values[i].first);
			if (row == NULL)
				continue;
			if (!allowedByConditions(*row, &made).ok())
				failing.push_back(made.values[i].first);
		}
		if (failing.empty())
		{
			batch = made;
			return;
		}

		bool progress = false;
		for (size_t f = 0; f < failing.size(); ++f)
		{
			if (eraseMember(named, failing[f]))
			{
				refuseByCondition(refused, failing[f],
					"the settings this one depends on do not allow it to be set");
				progress = true;
				continue;
			}

			/* Not named by the caller: a coupling put it. It is reported under its own key, and
			   what asked for it is refused with it, since one without the other is the state the
			   coupling exists to prevent. */
			refuseByCondition(refused, failing[f],
				"the settings this one depends on do not allow it to be set");
			for (size_t a = 0; a < added.size(); ++a)
			{
				if (added[a].key != failing[f])
					continue;
				if (eraseMember(named, added[a].trigger))
				{
					refuseByCondition(refused, added[a].trigger,
						"the setting it brings with it cannot be set");
					progress = true;
				}
			}
		}

		/* An addition nobody is recorded as owning cannot be traced to a member, so it alone
		   is left out rather than looping on it. */
		if (!progress)
		{
			for (size_t f = 0; f < failing.size(); ++f)
				eraseMember(made, failing[f]);
			batch = made;
			return;
		}
	}
}

void writeBatch(const std::vector<std::pair<std::string, std::string> > &members, Refusals &failed,
		bool onLoop)
{
	/* The three passes of one write, so every caller that writes more than one setting gets
	   the couplings and the settling and none can leave them out. The first refuses what is
	   wrong on its own, and only what passed goes into the batch, since a value that never
	   lands must not allow another one. */
	BatchOverlay batch;
	for (size_t i = 0; i < members.size(); ++i)
	{
		Result<void> checked = check(members[i].first, members[i].second);
		if (!checked.ok())
		{
			failed.push_back(std::make_pair(members[i].first, checked.error()));
			continue;
		}
		batch.values.push_back(members[i]);
	}

	settleBatch(batch, failed);

	for (size_t i = 0; i < batch.values.size(); ++i)
	{
		Result<void> done = set(batch.values[i].first, batch.values[i].second, &batch, onLoop);
		if (!done.ok())
			failed.push_back(std::make_pair(batch.values[i].first, done.error()));
	}
}

Result<void> clearSecret(const std::string &key)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	/* Only a credential, because this is the one call that writes a value no other call may
	   write. Widening it to every row would make it a second way to write a setting, one that
	   goes round the three rules the write beside it holds a value to. */
	if (!d->secret)
		return fail(Status::InvalidArgument, ErrorCode::NotACredential,
			    "the setting is not a credential and is not cleared here");

	/* Written as text whatever the row declares. A credential is a string on every row that
	   carries one, and a number row marked secret would have no empty value to write: there is
	   no such row, and if one were added the store would answer for it. */
	if (d->type != ValueType::String)
		return fail(Status::InvalidArgument, ErrorCode::NotACredential,
			    "the setting is a credential of a kind that has nothing to clear");

	// Emptying is a write like any other to a row whose rule wants more, such as a pin.
	if (d->text != NULL)
	{
		Result<void> rule = holdsTextRule(*d->text, std::string());
		if (!rule.ok())
			return fail(rule.error());
	}

	Status written = settingsSource().writeString(d->key, std::string());
	if (written != Status::Ok)
		return fail(written, ErrorCode::SettingNotWritten,
			    "the setting could not be written");

	// Saved here for the reason the write beside it saves: a value the store
	// took and nobody saved is gone the next time the program writes its file.
	Status saved = settingsSource().persist();
	if (saved != Status::Ok)
		return fail(saved, ErrorCode::SettingNotWritten,
			    "the setting was taken and not saved");

	return ok();
}

Result<std::vector<SettingChoice> > choices(const std::string &key)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

	std::vector<SettingChoice> out;
	if (!valuesOffered(*d, out))
		return fail(Status::InvalidArgument, ErrorCode::ChoicesUnavailable,
			    "the setting offers no set of values this box can state");

	return ok(std::move(out));
}

bool resolveLabel(const char *key, std::string &out)
{
	if (key == NULL)
		return false;
	return localeSource().text(key, out) == Status::Ok;
}

void announceSettingsChanged()
{
	Event e;
	e.type = EventType::SettingsChanged;
	EventBus::instance().publish(e);
}

} // namespace settings
} // namespace coreapi
