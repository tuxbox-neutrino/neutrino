/*
 * settings.cpp - reading and writing the box settings
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

#include "settings.h"

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "settingstable.h"

#include <vector>

#include <cerrno>
#include <cstdio>
#include <cstdlib>

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
}

/* Whether the row allows the value, which is not the question the store asks.
   That one is whether the field can hold it, and it is narrower. Both have to
   refuse what they refuse and neither stands in for the other, because a row's
   bounds can be wider than its field and a field is wider than most rows'
   bounds. */
Result<void> allowedByRow(const Descriptor &d, long value)
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
			if (value < d.min || value > d.max)
				return fail(Status::InvalidArgument, ErrorCode::OutOfRange,
					    "the setting takes " + decimal(d.min) + " to " + decimal(d.max));
			return ok();

		case ValueType::Enum:
			/* A row whose set is the box's own carries no list here, and the value is held to what
			   the box offers by the caller of this. Answering not a listed value from here would
			   refuse every value such a row has. */
			if (d.field.choices != NULL)
				return ok();
			for (size_t i = 0; d.values != NULL && i < d.value_count; ++i)
			{
				if (d.values[i].value == value)
					return ok();
			}
			return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
				    "the setting does not offer that value");

		case ValueType::String:
			break;
	}

	return fail(Status::Internal, ErrorCode::BadTable,
		    "the setting is not of a kind a number is offered for");
}

/* The values a row offers, whichever of the two places they come from. One
   function, because the write is held to the same set a read answers with:
   written twice, a caller could be offered a value the write turns down. False is
   a row that offers no set at all and a set nobody can state. */
bool valuesOffered(const Descriptor &d, std::vector<SettingChoice> &out)
{
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
		SettingChoice one;
		one.value = d.values[i].value;
		/* The text and not the name of it, because that is what the other kind
		   answers with and a caller must not have to tell the two apart. A name
		   the catalog carries nothing under leaves the text empty. */
		resolveLabel(d.values[i].label_key, one.label);
		listed.push_back(one);
	}
	out.swap(listed);
	return true;
}

} // anonymous namespace

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
		value = d->default_int;
	else if (s != Status::Ok)
		return fail(s, ErrorCode::SettingUnreadable,
			    "the setting could not be read");
	return ok(decimal(value));
}

Result<void> set(const std::string &key, const std::string &value)
{
	const Descriptor *d = row(key);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::UnknownSetting,
			    "no setting is declared under that key");

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

	Status written;
	if (d->type == ValueType::String)
	{
		Result<void> text = allowedAsText(value);
		if (!text.ok())
			return fail(text.error());

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

		written = settingsSource().writeString(d->key, value);
	}
	else
	{
		long number = 0;
		if (!parseNumber(value, number))
			return fail(Status::InvalidArgument, ErrorCode::NotANumber,
				    "the setting takes a whole number");

		Result<void> allowed = allowedByRow(*d, number);
		if (!allowed.ok())
			return fail(allowed.error());

		/* A row whose set is the box's own is held to what the box offers rather than to a list
		   nothing here has. Every value is refused while nobody can say what the set is: taking
		   one then would write a number the box cannot show, which is a picture nobody gets back
		   from with the remote control. */
		if (d->field.choices != NULL)
		{
			std::vector<SettingChoice> offered;
			if (!valuesOffered(*d, offered))
				return fail(Status::InvalidArgument, ErrorCode::ChoicesUnavailable,
					    "the values this setting offers are the box's own and nothing has said what they are");

			bool listed = false;
			for (size_t i = 0; !listed && i < offered.size(); ++i)
				listed = offered[i].value == number;
			if (!listed)
				return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
					    "the setting does not offer that value");
		}

		written = settingsSource().writeInt(d->key, number);
	}

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

} // namespace settings
} // namespace coreapi
